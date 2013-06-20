/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2010 -- 2013 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * DTrace Process Control
 *
 * This file provides a set of routines that permit libdtrace and its clients
 * to cache process state as necessary, and avoid repeatedly pulling the same
 * data out of /proc (which can be expensive).
 * The library provides several mechanisms in the libproc control layer:
 *
 * Process Control: For processes that are grabbed for control (~GRAB_RDONLY)
 * or created by dt_proc_create(), a control thread is created to provide
 * callbacks on process exit.  (FIXME: Use process groups instead.)
 *
 * MT-Safety: Libproc is not MT-Safe, so dt_proc_lock() and dt_proc_unlock()
 * are provided to synchronize access to the libproc handle between libdtrace
 * code and client code and the control thread's use of the ps_prochandle.
 *
 * NOTE: MT-Safety is NOT provided for libdtrace itself, or for use of the
 * dtrace_proc_grab/dtrace_proc_create mechanisms.  Like all exported libdtrace
 * calls, these are assumed to be MT-Unsafe.  MT-Safety is ONLY provided for
 * synchronization between libdtrace control threads and the client thread.
 *
 * The ps_prochandles themselves are maintained along with a dt_proc_t struct in
 * a hash table indexed by PID.  This provides basic locking and reference
 * counting.  The dt_proc_t is also maintained in LRU order on dph_lrulist.  The
 * dph_lrucnt and dph_lrulim count the number of processes we have grabbed or
 * created but not retired, and the current limit on the number of actively
 * cached entries.
 *
 * The control thread for a process currently invokes the process, resumes it
 * when dt_proc_continue() is called, then notifies interested parties when the
 * process dies.
 *
 * A simple notification mechanism is provided for libdtrace clients using
 * dtrace_handle_proc() for notification of process death.  When this event
 * occurs, the dt_proc_t itself is enqueued on a notification list and the
 * control thread broadcasts to dph_cv.  dtrace_sleep() will wake up using this
 * condition and will then call the client handler as necessary.
 */
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include <errno.h>
#include <port.h>
#include <poll.h>

#include <mutex.h>

#include <dt_proc.h>
#include <dt_pid.h>
#include <dt_impl.h>

static int dt_proc_attach(dt_proc_t *dpr, int before_continue);
static void dt_proc_remove(dtrace_hdl_t *dtp, pid_t pid);

static void
dt_proc_notify(dtrace_hdl_t *dtp, dt_proc_hash_t *dph, dt_proc_t *dpr,
    const char *msg)
{
	dt_proc_notify_t *dprn = dt_alloc(dtp, sizeof (dt_proc_notify_t));

	if (dprn == NULL) {
		dt_dprintf("failed to allocate notification for %d %s\n",
		    (int)dpr->dpr_pid, msg);
	} else {
		dprn->dprn_dpr = dpr;
		if (msg == NULL)
			dprn->dprn_errmsg[0] = '\0';
		else
			(void) strlcpy(dprn->dprn_errmsg, msg,
			    sizeof (dprn->dprn_errmsg));

		(void) pthread_mutex_lock(&dph->dph_lock);

		dprn->dprn_next = dph->dph_notify;
		dph->dph_notify = dprn;

		(void) pthread_cond_broadcast(&dph->dph_cv);
		(void) pthread_mutex_unlock(&dph->dph_lock);
	}
}

/*
 * Check to see if the control thread was requested to stop when the victim
 * process reached a particular event (why) rather than continuing the victim.
 * If 'why' is set in the stop mask, we wait on dpr_cv for dt_proc_continue().
 * If 'why' is not set, this function returns immediately and does nothing.
 */
static void
dt_proc_stop(dt_proc_t *dpr, uint8_t why)
{
	assert(MUTEX_HELD(&dpr->dpr_lock));
	assert(why != DT_PROC_STOP_IDLE);

	if (dpr->dpr_stop & why) {
		dpr->dpr_stop |= DT_PROC_STOP_IDLE;
		dpr->dpr_stop &= ~why;

		(void) pthread_cond_broadcast(&dpr->dpr_cv);

		while (dpr->dpr_stop & DT_PROC_STOP_IDLE)
			(void) pthread_cond_wait(&dpr->dpr_cv, &dpr->dpr_lock);
	}
}

/*
 * Fire a one-shot breakpoint to say that the child has got to an interesting
 * place from which we should grab control, possibly blocking.
 */
static int
dt_break_interesting(uintptr_t addr, void *dpr_data)
{
	dt_proc_t *dpr = dpr_data;

	dt_dprintf("pid %d: breakpoint\n", (int)dpr->dpr_pid);
	pthread_mutex_lock(&dpr->dpr_lock);
	dpr->dpr_tid_locked = B_TRUE;

	dt_proc_stop(dpr, dpr->dpr_hdl->dt_prcmode);
	Punbkpt(dpr->dpr_proc, addr);

	dpr->dpr_tid_locked = B_FALSE;
	pthread_mutex_unlock(&dpr->dpr_lock);
	return PS_RUN;
}

/*
 * A one-shot breakpoint that fires at a point at which the dynamic linker has
 * initialized far enough to enable us to do reliable symbol lookups, and thus
 * drop a breakpoint on main().
 */
static int
dt_break_drop_main(uintptr_t addr, void *dpr_data)
{
	dt_proc_t *dpr = dpr_data;
	int ret;

	ret = dt_proc_attach(dpr, B_FALSE);
	Punbkpt(dpr->dpr_proc, addr);

	/*
	 * If we couldn't dt_proc_attach(), because we couldn't find main();
	 * rendezvous here, instead.
	 */
	if (ret < 0) {
		pthread_mutex_lock(&dpr->dpr_lock);
		dpr->dpr_tid_locked = B_TRUE;

		dt_proc_stop(dpr, dpr->dpr_hdl->dt_prcmode);

		dpr->dpr_tid_locked = B_FALSE;
		pthread_mutex_unlock(&dpr->dpr_lock);
	}

	return PS_RUN;
}

static void
dt_proc_rdevent(rd_agent_t *rd, rd_event_msg_t *msg, void *state)
{
	dt_proc_t *dpr = state;
	dtrace_hdl_t *dtp = dpr->dpr_hdl;

	/*
	 * Ignore the state deallocation call.
	 */
	if (msg == NULL)
		return;

	dt_dprintf("pid %d: rtld event, type=%d state %d\n",
	    (int)dpr->dpr_pid, msg->type, msg->state);

	pthread_mutex_lock(&dpr->dpr_lock);
	dpr->dpr_tid_locked = B_TRUE;

	switch (msg->type) {
	case RD_DLACTIVITY:
		if (msg->state != RD_CONSISTENT)
			break;

		Pupdate_syms(dpr->dpr_proc);
		if (dt_pid_create_probes_module(dtp, dpr) != 0)
			dt_proc_notify(dtp, dtp->dt_procs, dpr,
			    dpr->dpr_errmsg);

		break;
	case RD_NONE:
		/* cannot happen, but do nothing anyway */
		break;
	}
	dpr->dpr_tid_locked = B_FALSE;
	pthread_mutex_unlock(&dpr->dpr_lock);
}

/*
 * Aarrange to be notified whenever the set of shared libraries in the child is
 * updated.
 */
static void
dt_proc_rdagent(dt_proc_t *dpr)
{
	/*
	 * TODO: this doesn't yet cope with statically linked programs, for
	 * which rd_event_enable() will return RD_NOMAPS until the first
	 * dlopen() happens, who knows how late into the program's execution.
	 *
	 * All of these calls are basically free if the agent already exists
	 * and monitoring is already active.
	 */
        if (Prd_agent(dpr->dpr_proc) != NULL)
		rd_event_enable(Prd_agent(dpr->dpr_proc), dt_proc_rdevent, dpr);
}

/*
 * Possibly arrange to stop the process, post-attachment, at the right place.
 * This may be called twice, before the dt_proc_continue() rendezvous just in
 * case the dynamic linker is far enough up to help us out, and from a
 * breakpoint set on preinit otherwise.
 *
 * Returns 0 on success, or -1 on failure (in which case the process is
 * still halted).
 */
static int
dt_proc_attach(dt_proc_t *dpr, int before_continue)
{
	uintptr_t addr = 0;
	GElf_Sym main_sym;
	dtrace_hdl_t *dtp = dpr->dpr_hdl;
	int (*handler) (uintptr_t addr, void *data) = dt_break_interesting;

	assert(MUTEX_HELD(&dpr->dpr_lock));

	dt_proc_rdagent(dpr);

	/*
	 * If we're stopping on exec we have no breakpoints to drop: if
	 * we're stopping on preinit and it's after the dt_proc_continue()
	 * rendezvous, we've already dropped the necessary breakpoints.
	 */

	if (dtp->dt_prcmode == DT_PROC_STOP_CREATE)
		return 0;

	if (!before_continue && dtp->dt_prcmode == DT_PROC_STOP_PREINIT)
		return 0;

	if (before_continue)
		/*
		 * Before dt_proc_continue().  Preinit, postinit and main all
		 * get a breakpoint dropped on the process entry point, though
		 * postinit and main use a different handler.
		 */
		switch (dtp->dt_prcmode) {
		case DT_PROC_STOP_POSTINIT:
		case DT_PROC_STOP_MAIN:
			handler = dt_break_drop_main;
		case DT_PROC_STOP_PREINIT:
			addr = Pgetauxval(dpr->dpr_proc, AT_ENTRY);
		}
	else
		/*
		 * After dt_proc_continue().  Postinit and main get a breakpoint
		 * dropped on main().
		 */
		switch (dtp->dt_prcmode) {
		case DT_PROC_STOP_POSTINIT:
		case DT_PROC_STOP_MAIN:
			if (Pxlookup_by_name(dpr->dpr_proc, LM_ID_BASE,
				PR_OBJ_EVERY, "main", &main_sym, NULL) == 0)
				addr = main_sym.st_value;
		}

	if (addr &&
	    Pbkpt(dpr->dpr_proc, addr, B_FALSE, handler, NULL, dpr) == 0)
		return 0;

	dt_dprintf("Cannot drop breakpoint in child process: acting as if "
	    "evaltime=%s were in force.\n", before_continue ? "exec" :
	    "preinit");
	dpr->dpr_stop &= ~dtp->dt_prcmode;
	if (before_continue) {
		dpr->dpr_stop |= DT_PROC_STOP_CREATE;
		dtp->dt_prcmode = DT_PROC_STOP_CREATE;
	} else {
		dpr->dpr_stop |= DT_PROC_STOP_PREINIT;
		dtp->dt_prcmode = DT_PROC_STOP_PREINIT;
	}

	return -1;
}

/*PRINTFLIKE3*/
_dt_printflike_(3,4)
static struct ps_prochandle *
dt_proc_error(dtrace_hdl_t *dtp, dt_proc_t *dpr, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	dt_set_errmsg(dtp, NULL, NULL, NULL, 0, format, ap);
	va_end(ap);

	(void) dt_set_errno(dtp, EDT_COMPILER);
	return (NULL);
}

typedef struct dt_proc_control_data {
	dtrace_hdl_t *dpcd_hdl;			/* DTrace handle */
	dt_proc_t *dpcd_proc;			/* process to control */
	/*
	 * The next two are only valid while the master thread is calling
	 * dt_proc_create(), and only useful when dpr_created is true.
	 */
	const char *dpcd_start_proc;
	char * const *dpcd_start_proc_argv;
} dt_proc_control_data_t;

static void dt_proc_control_cleanup(void *arg);

/*
 * Main loop for all victim process control threads.  We initialize all the
 * appropriate /proc control mechanisms, start the process and halt it, notify
 * the caller of this, then wait for the caller to indicate its readiness and
 * resume the process: only then do we enter a loop waiting for the process to
 * stop on an event or die.  We process any events by calling appropriate
 * subroutines, and exit when the victim dies or we lose control.
 *
 * The control thread synchronizes the use of dpr_proc with other libdtrace
 * threads using dpr_lock.  We hold the lock for all of our operations except
 * waiting while the process is running.  If the libdtrace client wishes to exit
 * or abort our wait, thread cancellation can be used.
 */
static void *
dt_proc_control(void *arg)
{
	dt_proc_control_data_t *datap = arg;
	dtrace_hdl_t *dtp = datap->dpcd_hdl;
	dt_proc_t *dpr = datap->dpcd_proc;
	dt_proc_hash_t *dph = dpr->dpr_hdl->dt_procs;
	int err;
	pid_t pid;
	struct ps_prochandle *P;

	/*
	 * Note: this function must not exit before dt_proc_stop() is called, or
	 * dt_proc_create_thread() will hang indefinitely.
	 */

	/*
	 * Arrange to clean up when cancelled by dt_proc_destroy() on shutdown.
	 */
	pthread_cleanup_push(dt_proc_control_cleanup, dpr);

	/*
	 * Lock our mutex, preventing races between cv broadcasts to our
	 * controlling thread and dt_proc_continue() or process destruction.
	 */
	pthread_mutex_lock(&dpr->dpr_lock);
	dpr->dpr_tid_locked = B_TRUE;

	/*
	 * Either create the process, or grab it.  Whichever, on failure, quit
	 * and let our cleanup run (signalling failure to
	 * dt_proc_create_thread() in the process).
	 *
	 * At this point, the process is halted at exec(), if created.
	 */

	if (dpr->dpr_created) {
		if ((dpr->dpr_proc = Pcreate(datap->dpcd_start_proc,
			    datap->dpcd_start_proc_argv, &err, 0)) == NULL) {
			dt_proc_error(dtp, dpr, "failed to execute %s: %s\n",
			    datap->dpcd_start_proc, strerror(err));
			pthread_exit(NULL);
		}
		dpr->dpr_pid = Pgetpid(dpr->dpr_proc);
	} else {
		if ((dpr->dpr_proc = Pgrab(dpr->dpr_pid, &err)) == NULL) {
			dt_proc_error(dtp, dpr, "failed to grab pid %li: %s\n",
			    (long) dpr->dpr_pid, strerror(err));
			dt_dprintf("grab failure\n");
			pthread_exit(NULL);
		}
	}

	pid = dpr->dpr_pid;
	P = dpr->dpr_proc;

	/*
	 * Enable rtld breakpoints at the location specified by dt_prcmode (or
	 * drop other breakpoints which will eventually enable us to drop
	 * breakpoints at that location).
	 */
	dt_proc_attach(dpr, B_TRUE);

	/*
	 * Wait for a rendezvous from dt_proc_continue().  After this point,
	 * datap and all that it points to is no longer valid.
	 *
	 * This covers evaltime=exec and grabs, but not the three evaltimes that
	 * depend on breakpoints.  Those wait for rendezvous from within the
	 * breakpoint handler, invoked from Pwait() below.
	 */
	if (dpr->dpr_created)
		dt_proc_stop(dpr, DT_PROC_STOP_CREATE);
	else
		dt_proc_stop(dpr, DT_PROC_STOP_GRAB);

	Ptrace_set_detached(P, dpr->dpr_created);
	Puntrace(P, 0);

	dpr->dpr_tid_locked = B_FALSE;
	pthread_mutex_unlock(&dpr->dpr_lock);

	/*
	 * Wait for the process corresponding to this control thread to stop,
	 * process the event, and then set it running again.  We want to sleep
	 * with dpr_lock *unheld* so that other parts of libdtrace can use the
	 * ps_prochandle in the meantime (e.g. ustack()) without corrupting any
	 * shared caches.  We do not expect them to modify the Pstate(), so we
	 * can call that without grabbing the lock.
	 *
	 * TODO: this is impossible to implement properly at present (and
	 * useless, since other parts of libdtrace cannot use the ps_prochandle
	 * at all), but will be fixed once waitfds are in use.
	 */
	for (;;) {
		if (Pwait(P, B_TRUE) == -1 && errno == EINTR)
			continue; /* hit by signal: continue waiting */

		/*
		 * If we don't yet have an rtld_db handle, try again to get one.
		 * (ld.so can take arbitrarily long to get ready for this.)
		 */
		dt_proc_rdagent(dpr);

		switch (Pstate(P)) {
		case PS_STOP:

			/*
			 * If the process stops showing one of the events that
			 * we are tracing, perform the appropriate response.
			 *
			 * TODO: the stop() action may need some work here.
			 */
			continue;

			/*
			 * If the libdtrace caller (as opposed to any other
			 * process) tries to debug a monitored process, it
			 * may lead to our waitpid() returning strange
			 * results.  Fail in this case, until we define a
			 * protocol for communicating the waitpid() results to
			 * the caller, or relinquishing control temporarily.
			 * FIXME.
			 */
		case PS_TRACESTOP:
			dt_dprintf("pid %d: trace stop, nothing we can do\n",
			    pid);
			continue;

		case PS_DEAD:
			dt_dprintf("pid %d: proc died\n", pid);
			break;
		}

		if (Pstate(P) == PS_DEAD)
			break;
	}

	/*
	 * We only get here if the monitored process has died.  Enqueue the
	 * dt_proc_t structure on the dt_proc_hash_t notification list, then
	 * clean up.
	 */
	pthread_mutex_lock(&dph->dph_destroy_lock);

	dpr->dpr_proc = NULL;
	dt_proc_notify(dtp, dph, dpr, NULL);

	pthread_cleanup_pop(1);

	dt_list_delete(&dph->dph_lrulist, dpr);
	dt_proc_remove(dtp, pid);
	/* dt_free(dtp, dpr); -- XXX temporarily diked out */

	pthread_mutex_unlock(&dph->dph_destroy_lock);

	return (NULL);
}

/*
 * Cleanup handler, called when a process control thread exits or is cancelled.
 */
static void
dt_proc_control_cleanup(void *arg)
{
	dt_proc_t *dpr = arg;

	/*
	 * Set dpr_done and clear dpr_tid to indicate that the control thread
	 * has exited, and notify any waiting thread in dt_proc_destroy() that
	 * we have successfully exited.
	 *
	 * If we were cancelled while already holding the mutex, don't lock it
	 * again.
 	 */
	if(!dpr->dpr_tid_locked) {
		pthread_mutex_lock(&dpr->dpr_lock);
		dpr->dpr_tid_locked = B_TRUE;
	}

	if (dpr->dpr_proc) {
		Prelease(dpr->dpr_proc, dpr->dpr_created);
		dpr->dpr_proc = NULL;
	}

	dpr->dpr_done = B_TRUE;
	dpr->dpr_tid = 0;
	pthread_cond_broadcast(&dpr->dpr_cv);

	dpr->dpr_tid_locked = B_FALSE;
	pthread_mutex_unlock(&dpr->dpr_lock);
}

dt_proc_t *
dt_proc_lookup(dtrace_hdl_t *dtp, struct ps_prochandle *P, int remove)
{
	dt_proc_hash_t *dph = dtp->dt_procs;
	pid_t pid = Pgetpid(P);
	dt_proc_t *dpr, **dpp = &dph->dph_hash[pid & (dph->dph_hashlen - 1)];

	for (dpr = *dpp; dpr != NULL; dpr = dpr->dpr_hash) {
		if (dpr->dpr_pid == pid)
			break;
		else
			dpp = &dpr->dpr_hash;
	}

	assert(dpr != NULL);
	assert(dpr->dpr_proc == P);

	if (remove)
		*dpp = dpr->dpr_hash; /* remove from pid hash chain */

	return (dpr);
}

static void
dt_proc_remove(dtrace_hdl_t *dtp, pid_t pid)
{
	dt_proc_hash_t *dph = dtp->dt_procs;
	dt_proc_t *dpr, **dpp = &dph->dph_hash[pid & (dph->dph_hashlen - 1)];

	for (dpr = *dpp; dpr != NULL; dpr = dpr->dpr_hash) {
		if (dpr->dpr_pid == pid)
			break;
		else
			dpp = &dpr->dpr_hash;
	}
	if (dpr == NULL)
		return;

	*dpp = dpr->dpr_hash;
}

/*
 * Retirement of a process happens after a long period of nonuse, and serves to
 * reduce the OS impact of process management of such processes.  A virtually
 * unlimited number of processes may exist in retired state at any one time:
 * they come out of retirement automatically when they are used again.
 */
static void
dt_proc_retire(struct ps_prochandle *P)
{
	(void) Pclose(P);
}

/*
 * Determine if a process is retired.  Very cheap.
 */
static int
dt_proc_retired(struct ps_prochandle *P)
{
	return (!Phasfds(P));
}

static void
dt_proc_destroy(dtrace_hdl_t *dtp, struct ps_prochandle *P)
{
	dt_proc_t *dpr = dt_proc_lookup(dtp, P, B_FALSE);
	dt_proc_hash_t *dph = dtp->dt_procs;
	dt_proc_notify_t *npr, **npp;

	/*
	 * Before we free the process structure, remove this dt_proc_t from the
	 * lookup hash, and then walk the dt_proc_hash_t's notification list
	 * and remove this dt_proc_t if it is enqueued.  If the dpr is already
	 * gone, do nothing.
	 */
	if (dpr == NULL)
		return;

	dt_dprintf("%s pid %d\n", dpr->dpr_created ? "killing" : "releasing",
		dpr->dpr_pid);

	pthread_mutex_lock(&dph->dph_lock);
	dt_proc_lookup(dtp, P, B_TRUE);
	npp = &dph->dph_notify;

	while ((npr = *npp) != NULL) {
		if (npr->dprn_dpr == dpr) {
			*npp = npr->dprn_next;
			dt_free(dtp, npr);
		} else {
			npp = &npr->dprn_next;
		}
	}

	pthread_mutex_unlock(&dph->dph_lock);
	if (dpr->dpr_tid) {
		/*
		 * If the process is currently waiting in dt_proc_stop(), poke it
		 * into running again.  This is essential to ensure that the
		 * thread cancel does not hit while it already has the mutex
		 * locked.
		 */
		if (dpr->dpr_stop & DT_PROC_STOP_IDLE) {
			dpr->dpr_stop &= ~DT_PROC_STOP_IDLE;
			pthread_cond_broadcast(&dpr->dpr_cv);
		}

		/*
		 * Cancel the daemon thread, then wait for dpr_done to indicate
		 * the thread has exited.  (This will also terminate the
		 * process.)
		 *
		 * Do not use P below this point: it has been freed by the
		 * daemon's cleanup process.
		 */
		pthread_mutex_lock(&dpr->dpr_lock);
		pthread_cancel(dpr->dpr_tid);

		while (!dpr->dpr_done)
			(void) pthread_cond_wait(&dpr->dpr_cv, &dpr->dpr_lock);

		pthread_mutex_unlock(&dpr->dpr_lock);
	} else
		/*
		 * No daemon thread to clean up for us. Prelease() the
		 * underlying process explicitly.
		 */
		Prelease(P, dpr->dpr_created);

	assert(dph->dph_lrucnt != 0);
	dph->dph_lrucnt--;
	dt_list_delete(&dph->dph_lrulist, dpr);
	dt_proc_remove(dtp, dpr->dpr_pid);
	dt_free(dtp, dpr);
}

static int
dt_proc_create_thread(dtrace_hdl_t *dtp, dt_proc_t *dpr, uint_t stop,
    const char *file, char *const *argv)
{
	dt_proc_control_data_t data;
	sigset_t nset, oset;
	pthread_attr_t a;
	int err;

	(void) pthread_mutex_lock(&dpr->dpr_lock);
	dpr->dpr_stop |= stop; /* set bit for initial rendezvous */

	(void) pthread_attr_init(&a);
	(void) pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED);

	(void) sigfillset(&nset);
	(void) sigdelset(&nset, SIGABRT);	/* unblocked for assert() */

	data.dpcd_hdl = dtp;
	data.dpcd_proc = dpr;
	data.dpcd_start_proc = file;
	data.dpcd_start_proc_argv = argv;

	(void) pthread_sigmask(SIG_SETMASK, &nset, &oset);
	err = pthread_create(&dpr->dpr_tid, &a, dt_proc_control, &data);
	(void) pthread_sigmask(SIG_SETMASK, &oset, NULL);

	/*
	 * If the control thread was created, then wait on dpr_cv for either
	 * dpr_done to be set (the victim died or the control thread failed) or
	 * DT_PROC_STOP_IDLE to be set, indicating that the victim is now
	 * stopped and the control thread is at the rendezvous event.  On
	 * success, we return with the process and control thread stopped: the
	 * caller can then apply dt_proc_continue() to resume both.
	 */
	if (err == 0) {
		while (!dpr->dpr_done && !(dpr->dpr_stop & DT_PROC_STOP_IDLE))
			(void) pthread_cond_wait(&dpr->dpr_cv, &dpr->dpr_lock);

		/*
		 * If dpr_done is set, the control thread aborted before it
		 * reached the rendezvous event.  We try to provide a small
		 * amount of useful information to help figure out why.
		 */
		if (dpr->dpr_done) {
			err = ESRCH; /* cause grab() or create() to fail */
		}
	} else {
		(void) dt_proc_error(dpr->dpr_hdl, dpr,
		    "failed to create control thread for process-id %d: %s\n",
		    (int)dpr->dpr_pid, strerror(err));
	}

	(void) pthread_mutex_unlock(&dpr->dpr_lock);
	(void) pthread_attr_destroy(&a);

	if (err != 0)
		dt_free(dtp, dpr);

	return (err);
}

struct ps_prochandle *
dt_proc_create(dtrace_hdl_t *dtp, const char *file, char *const *argv)
{
	dt_proc_hash_t *dph = dtp->dt_procs;
	dt_proc_t *dpr;

	if ((dpr = dt_zalloc(dtp, sizeof (dt_proc_t))) == NULL)
		return (NULL); /* errno is set for us */

	(void) pthread_mutex_init(&dpr->dpr_lock, NULL);
	(void) pthread_cond_init(&dpr->dpr_cv, NULL);

	dpr->dpr_hdl = dtp;
	dpr->dpr_created = B_TRUE;

	if (dt_proc_create_thread(dtp, dpr, dtp->dt_prcmode,
		file, argv) != 0)
		return (NULL); /* dt_proc_error() has been called for us */

	dph->dph_lrucnt++;
	dpr->dpr_hash = dph->dph_hash[dpr->dpr_pid & (dph->dph_hashlen - 1)];
	dph->dph_hash[dpr->dpr_pid & (dph->dph_hashlen - 1)] = dpr;
	dt_list_prepend(&dph->dph_lrulist, dpr);

	dt_dprintf("created pid %d\n", (int)dpr->dpr_pid);
	dpr->dpr_refs++;

	return (dpr->dpr_proc);
}

struct ps_prochandle *
dt_proc_grab(dtrace_hdl_t *dtp, pid_t pid)
{
	dt_proc_hash_t *dph = dtp->dt_procs;
	uint_t h = pid & (dph->dph_hashlen - 1);
	dt_proc_t *dpr, *opr;

	/*
	 * Search the hash table for the pid.  If it is already grabbed or
	 * created, move the handle to the front of the lrulist, increment
	 * the reference count, and return the existing ps_prochandle.
	 *
	 * If it is retired, bring it out of retirement aggressively, so as to
	 * ensure that dph_lrucnt and dt_proc_retired() do not get out of synch
	 * (which would cause aggressive early retirement of processes even when
	 * unnecessary).
	 *
	 * Since nothing ever removes the process from this list until
	 * dt_proc_hash_destroy() / dtrace_close(), it is impossible for a
	 * process to be Pgrab()bed or Pcreate()d more than once: thus it will
	 * only be stopped once, and we do not need to deal with the downsides
	 * of ptrace() in normal operation.
	 */
	for (dpr = dph->dph_hash[h]; dpr != NULL; dpr = dpr->dpr_hash) {
		if (dpr->dpr_pid == pid) {
			dt_dprintf("grabbed pid %d (cached)\n", (int)pid);
			dt_list_delete(&dph->dph_lrulist, dpr);
			dt_list_prepend(&dph->dph_lrulist, dpr);
			dpr->dpr_refs++;
			if (dt_proc_retired(dpr->dpr_proc)) {
				/* not retired any more */
				(void) Pmemfd(dpr->dpr_proc);
				dph->dph_lrucnt++;
			}
			return (dpr->dpr_proc);
		}
	}

	if ((dpr = dt_zalloc(dtp, sizeof (dt_proc_t))) == NULL)
		return (NULL); /* errno is set for us */

	(void) pthread_mutex_init(&dpr->dpr_lock, NULL);
	(void) pthread_cond_init(&dpr->dpr_cv, NULL);

	dpr->dpr_hdl = dtp;
	dpr->dpr_pid = pid;
	dpr->dpr_created = B_FALSE;

	/*
	 * Create a control thread for this process and store its ID in
	 * dpr->dpr_tid.
	 */
	if (dt_proc_create_thread(dtp, dpr, DT_PROC_STOP_GRAB, NULL, NULL) != 0)
		return (NULL); /* dt_proc_error() has been called for us */

	/*
	 * If we're currently caching more processes than dph_lrulim permits,
	 * attempt to find the least-recently-used process that is currently
	 * unreferenced and has not already been retired, and retire it.  (This
	 * does not actually delete it, because its presence is still necessary
	 * to ensure that we do put it into halted state again.  It merely
	 * closes any associated filehandles.)
	 *
	 * We know this expiry run cannot affect the handle currently being
	 * grabbed, or we'd have boosted its refcnt and returned already.
	 */
	if (dph->dph_lrucnt >= dph->dph_lrulim) {
		for (opr = dt_list_prev(&dph->dph_lrulist);
		     opr != NULL; opr = dt_list_prev(opr)) {
			if (opr->dpr_refs == 0 && !dt_proc_retired(opr->dpr_proc)) {
				dt_proc_retire(opr->dpr_proc);
				dph->dph_lrucnt--;
				break;
			}
		}
	}

	dph->dph_lrucnt++;
	dpr->dpr_hash = dph->dph_hash[h];
	dph->dph_hash[h] = dpr;
	dt_list_prepend(&dph->dph_lrulist, dpr);

	dt_dprintf("grabbed pid %d\n", (int)pid);
	dpr->dpr_refs++;

	return (dpr->dpr_proc);
}

void
dt_proc_release(dtrace_hdl_t *dtp, struct ps_prochandle *P)
{
	dt_proc_t *dpr = dt_proc_lookup(dtp, P, B_FALSE);
	dt_proc_hash_t *dph = dtp->dt_procs;

	assert(dpr != NULL);
	assert(dpr->dpr_refs != 0);

	if (--dpr->dpr_refs == 0 &&
	    (dph->dph_lrucnt > dph->dph_lrulim) &&
	    !dt_proc_retired(dpr->dpr_proc)) {
		dt_proc_retire(P);
		dph->dph_lrucnt--;
	}
}

void
dt_proc_continue(dtrace_hdl_t *dtp, struct ps_prochandle *P)
{
	dt_proc_t *dpr = dt_proc_lookup(dtp, P, B_FALSE);

	(void) pthread_mutex_lock(&dpr->dpr_lock);

	if (dpr->dpr_stop & DT_PROC_STOP_IDLE) {
		dpr->dpr_stop &= ~DT_PROC_STOP_IDLE;
		(void) pthread_cond_broadcast(&dpr->dpr_cv);
	}

	(void) pthread_mutex_unlock(&dpr->dpr_lock);
}

void
dt_proc_lock(dtrace_hdl_t *dtp, struct ps_prochandle *P)
{
	dt_proc_t *dpr = dt_proc_lookup(dtp, P, B_FALSE);
	int err = pthread_mutex_lock(&dpr->dpr_lock);
	assert(err == 0); /* check for recursion */
}

void
dt_proc_unlock(dtrace_hdl_t *dtp, struct ps_prochandle *P)
{
	dt_proc_t *dpr = dt_proc_lookup(dtp, P, B_FALSE);
	int err = pthread_mutex_unlock(&dpr->dpr_lock);
	assert(err == 0); /* check for unheld lock */
}

void
dt_proc_hash_create(dtrace_hdl_t *dtp)
{
	if ((dtp->dt_procs = dt_zalloc(dtp, sizeof (dt_proc_hash_t) +
	    sizeof (dt_proc_t *) * _dtrace_pidbuckets - 1)) != NULL) {

		(void) pthread_mutex_init(&dtp->dt_procs->dph_lock, NULL);
		(void) pthread_mutex_init(&dtp->dt_procs->dph_destroy_lock, NULL);
		(void) pthread_cond_init(&dtp->dt_procs->dph_cv, NULL);

		dtp->dt_procs->dph_hashlen = _dtrace_pidbuckets;
		dtp->dt_procs->dph_lrulim = _dtrace_pidlrulim;
	}
}

void
dt_proc_hash_destroy(dtrace_hdl_t *dtp)
{
	dt_proc_hash_t *dph = dtp->dt_procs;
	dt_proc_t *dpr;

	pthread_mutex_lock(&dph->dph_destroy_lock);
	while ((dpr = dt_list_next(&dph->dph_lrulist)) != NULL)
		dt_proc_destroy(dtp, dpr->dpr_proc);
	pthread_mutex_unlock(&dph->dph_destroy_lock);

	dtp->dt_procs = NULL;
	dt_free(dtp, dph);
}

struct ps_prochandle *
dtrace_proc_create(dtrace_hdl_t *dtp, const char *file, char *const *argv)
{
	dt_ident_t *idp = dt_idhash_lookup(dtp->dt_macros, "target");
	struct ps_prochandle *P = dt_proc_create(dtp, file, argv);

	if (P != NULL && idp != NULL && idp->di_id == 0)
		idp->di_id = Pgetpid(P); /* $target = created pid */

	return (P);
}

struct ps_prochandle *
dtrace_proc_grab(dtrace_hdl_t *dtp, pid_t pid, int flags)
{
	dt_ident_t *idp = dt_idhash_lookup(dtp->dt_macros, "target");
	struct ps_prochandle *P = dt_proc_grab(dtp, pid);

	if (P != NULL && idp != NULL && idp->di_id == 0)
		idp->di_id = pid; /* $target = grabbed pid */

	return (P);
}

void
dtrace_proc_release(dtrace_hdl_t *dtp, struct ps_prochandle *P)
{
	dt_proc_release(dtp, P);
}

void
dtrace_proc_continue(dtrace_hdl_t *dtp, struct ps_prochandle *P)
{
	dt_proc_continue(dtp, P);
}
