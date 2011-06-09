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
 * Copyright 2010 Sun Microsystems, Inc.  All rights reserved.
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
 * XXX is this still true?
 *
 * NOTE: MT-Safety is NOT provided for libdtrace itself, or for use of the
 * dtrace_proc_grab/dtrace_proc_create mechanisms.  Like all exported libdtrace
 * calls, these are assumed to be MT-Unsafe.  MT-Safety is ONLY provided for
 * synchronization between libdtrace control threads and the client thread.
 *
 * The ps_prochandles themselves are maintained along with a dt_proc_t struct
 * in a hash table indexed by PID.  This provides basic locking and reference
 * counting.  The dt_proc_t is also maintained in LRU order on dph_lrulist.
 * The dph_lrucnt and dph_lrulim count the number of cacheable processes and
 * the current limit on the number of actively cached entries.
 *
 * The control thread for a process currently does nothing but notify interested
 * parties when the process dies.
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

/*ARGSUSED*/
static void
dt_proc_bpmain(dtrace_hdl_t *dtp, dt_proc_t *dpr, const char *fname)
{
	dt_dprintf("pid %d: breakpoint at %s()\n", (int)dpr->dpr_pid, fname);
	dt_proc_stop(dpr, DT_PROC_STOP_MAIN);
}

/*
 * Common code for enabling events associated with the run-time linker after
 * attaching to a process or after a victim process completes an exec(2).
 */
static void
dt_proc_attach(dt_proc_t *dpr, int exec)
{
	rd_err_e err;
	GElf_Sym sym;

	assert(MUTEX_HELD(&dpr->dpr_lock));

	if (exec) {
		Preset_maps(dpr->dpr_proc);
	}

#if 0
        /* FIXME: We want to use static tracepoints for this. */

        if ((dpr->dpr_rtld = Prd_agent(dpr->dpr_proc)) != NULL &&
	    (err = rd_event_enable(dpr->dpr_rtld, B_TRUE)) == RD_OK) {
		dt_proc_rdwatch(dpr, RD_PREINIT, "RD_PREINIT");
		dt_proc_rdwatch(dpr, RD_POSTINIT, "RD_POSTINIT");
		dt_proc_rdwatch(dpr, RD_DLACTIVITY, "RD_DLACTIVITY");
	} else {
		dt_dprintf("pid %d: failed to enable rtld events: %s\n",
		    (int)dpr->dpr_pid, dpr->dpr_rtld ? rd_errstr(err) :
		    "rtld_db agent initialization failed");
	}

	Pupdate_maps(dpr->dpr_proc);

	/* FIXME: put a breakpoint on main() */

#endif
}

/*
 * Wait for a stopped process to be set running again by some other debugger.
 * This is typically not required by /proc-based debuggers, since the usual
 * model is that one debugger controls one victim.  But DTrace, as usual, has
 * its own needs: the stop() action assumes that prun(1) or some other tool
 * will be applied to resume the victim process.  This could be solved by
 * adding a PCWRUN directive to /proc, but that seems like overkill unless
 * other debuggers end up needing this functionality, so we implement a cheap
 * equivalent to PCWRUN using the set of existing kernel mechanisms.
 *
 * Our intent is really not just to wait for the victim to run, but rather to
 * wait for it to run and then stop again for a reason other than the current
 * PR_REQUESTED stop.  As each Pwait() waits until a stop, we can just
 * repeatedly Pwait().  dt_proc_control() will then rediscover the new state and
 * continue as usual.  When the process is still stopped in the same exact
 * state, we sleep for a brief interval before waiting again so as not to spin
 * consuming CPU cycles.
 */
static void
dt_proc_waitrun(dt_proc_t *dpr)
{
	/* FIXME: Not implementable on Linux. */
}

typedef struct dt_proc_control_data {
	dtrace_hdl_t *dpcd_hdl;			/* DTrace handle */
	dt_proc_t *dpcd_proc;			/* proccess to control */
} dt_proc_control_data_t;

/*
 * Main loop for all victim process control threads.  We initialize all the
 * appropriate /proc control mechanisms, and then enter a loop waiting for
 * the process to stop on an event or die.  We process any events by calling
 * appropriate subroutines, and exit when the victim dies or we lose control.
 *
 * The control thread synchronizes the use of dpr_proc with other libdtrace
 * threads using dpr_lock.  We hold the lock for all of our operations except
 * waiting while the process is running.  If the libdtrace client wishes to exit
 * or abort our wait, SIGCANCEL can be used.
 */
static void *
dt_proc_control(void *arg)
{
	dt_proc_control_data_t *datap = arg;
	dtrace_hdl_t *dtp = datap->dpcd_hdl;
	dt_proc_t *dpr = datap->dpcd_proc;
	dt_proc_hash_t *dph = dpr->dpr_hdl->dt_procs;
	struct ps_prochandle *P = dpr->dpr_proc;

	int pid = dpr->dpr_pid;

	int notify = B_FALSE;

	/*
	 * We disable the POSIX thread cancellation mechanism so that the
	 * client program using libdtrace can't accidentally cancel our thread.
	 * dt_proc_destroy() uses SIGCANCEL explicitly to simply poke us out
	 * of PCWSTOP with EINTR, at which point we will see dpr_quit and exit.
	 */
	(void) pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

	/*
	 * Lock our mutex.
	 */
	(void) pthread_mutex_lock(&dpr->dpr_lock);

	/* FIXME: turn on synchronous mode here, when implemented: remember to
	 * adjust breakpoint EIP. */

#ifdef USERSPACE_TRACEPOINTS
	dt_proc_attach(dpr, B_FALSE);		/* enable rtld breakpoints */
#endif

	/*
	 * Check for an appropriate stop request and wait for dt_proc_continue.
	 */
	if (dpr->dpr_created)
		dt_proc_stop(dpr, DT_PROC_STOP_CREATE);
	else
		dt_proc_stop(dpr, DT_PROC_STOP_GRAB);

	if (Psetrun(P, 0, 0) == -1) {
		dt_dprintf("pid %d: failed to set running: %s\n",
		    (int)dpr->dpr_pid, strerror(errno));
	}

	(void) pthread_mutex_unlock(&dpr->dpr_lock);

	/*
	 * Wait for the process corresponding to this control thread to stop,
	 * process the event, and then set it running again.  We want to sleep
	 * with dpr_lock *unheld* so that other parts of libdtrace can use the
	 * ps_prochandle in the meantime (e.g. ustack()).  To do this, we wait
	 * for /proc/<pid>ctl using Pwait(). Once the process stops, we wake
	 * up, grab dpr_lock, and then call Pwait() (which will return
	 * immediately) and do our processing.
	 */
	while (!dpr->dpr_quit) {

		if (Pwait (P, 0) == -1 && errno == EINTR)
			continue; /* check dpr_quit and continue waiting */

		(void) pthread_mutex_lock(&dpr->dpr_lock);

		switch (Pstate(P)) {
		case PS_STOP:

			/*
			 * FIXME: handle other debuggers interrupting us here.
			 * Also distinguish stop() from normal stops (how?)
			 */

			/*
			 * If the process stops showing one of the events that
			 * we are tracing, perform the appropriate response.
			 * Note that we ignore PR_SUSPENDED, PR_CHECKPOINT, and
			 * PR_JOBCONTROL by design: if one of these conditions
			 * occurs, we will fall through to Psetrun() but the
			 * process will remain stopped in the kernel by the
			 * corresponding mechanism (e.g. job control stop).
			 *
			 * FIXME: This used to be done using breakpoints.
			 */
			break;

		case PS_DEAD:
			dt_dprintf("pid %d: proc died\n", pid);
			dpr->dpr_quit = B_TRUE;
			notify = B_TRUE;
			break;
		}

		(void) pthread_mutex_unlock(&dpr->dpr_lock);
	}

	/*
	 * If the control thread detected PS_UNDEAD or PS_DEAD, then enqueue the
	 * dt_proc_t structure on the dt_proc_hash_t notification list.
	 */
	if (notify)
		dt_proc_notify(dtp, dph, dpr, NULL);

	/*
	 * Destroy and remove any remaining breakpoints, set dpr_done and clear
	 * dpr_tid to indicate the control thread has exited, and notify any
	 * waiting thread in dt_proc_destroy() that we have succesfully exited.
	 */
	(void) pthread_mutex_lock(&dpr->dpr_lock);

	dpr->dpr_done = B_TRUE;
	dpr->dpr_tid = 0;

	(void) pthread_cond_broadcast(&dpr->dpr_cv);
	(void) pthread_mutex_unlock(&dpr->dpr_lock);

	return (NULL);
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

	if (dpr->dpr_proc != NULL)
		Prelease(dpr->dpr_proc, 0);

	dt_free(dtp, dpr);
	(void) dt_set_errno(dtp, EDT_COMPILER);
	return (NULL);
}

dt_proc_t *
dt_proc_lookup(dtrace_hdl_t *dtp, struct ps_prochandle *P, int remove)
{
	dt_proc_hash_t *dph = dtp->dt_procs;
	pid_t pid = ps_getpid(P);
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
dt_proc_destroy(dtrace_hdl_t *dtp, struct ps_prochandle *P)
{
	dt_proc_t *dpr = dt_proc_lookup(dtp, P, B_FALSE);
	dt_proc_hash_t *dph = dtp->dt_procs;
	dt_proc_notify_t *npr, **npp;
	boolean_t kill_it;

	assert(dpr != NULL);

	dt_dprintf("%s pid %d\n", dpr->dpr_created ? "killing" : "releasing",
		dpr->dpr_pid);
	kill_it = dpr->dpr_created;

	if (dpr->dpr_tid) {
		/*
		 * Set the dpr_quit flag to tell the daemon thread to exit.  We
		 * send it a SIGCANCEL to poke it out of a ptrace() wait.  Our
		 * daemon threads have POSIX cancellation disabled, so EINTR
		 * will be the only effect.  We then wait for dpr_done to
		 * indicate the thread has exited.
		 *
		 * We can't use pthread_kill() to send SIGCANCEL because the
		 * interface forbids it and we can't use pthread_cancel()
		 * because with cancellation disabled it won't actually
		 * send SIGCANCEL to the target thread, so we use _lwp_kill()
		 * to do the job.  This is all built on evil knowledge of
		 * the details of the cancellation mechanism in libc.
		 */
		(void) pthread_mutex_lock(&dpr->dpr_lock);
		dpr->dpr_quit = B_TRUE;
#if defined(sun)
		(void) _lwp_kill(dpr->dpr_tid, SIGCANCEL);
#else
		(void) pthread_kill(dpr->dpr_tid, SIGUSR1);
#endif

		/*
		 * If the process is currently idling in dt_proc_stop(), poke it
		 * into running again.
		 */
		if (dpr->dpr_stop & DT_PROC_STOP_IDLE) {
			dpr->dpr_stop &= ~DT_PROC_STOP_IDLE;
			(void) pthread_cond_broadcast(&dpr->dpr_cv);
		}

		while (!dpr->dpr_done)
			(void) pthread_cond_wait(&dpr->dpr_cv, &dpr->dpr_lock);

		(void) pthread_mutex_unlock(&dpr->dpr_lock);
	}

	/*
	 * Before we free the process structure, remove this dt_proc_t from the
	 * lookup hash, and then walk the dt_proc_hash_t's notification list
	 * and remove this dt_proc_t if it is enqueued.
	 */
	(void) pthread_mutex_lock(&dph->dph_lock);
	(void) dt_proc_lookup(dtp, P, B_TRUE);
	npp = &dph->dph_notify;

	while ((npr = *npp) != NULL) {
		if (npr->dprn_dpr == dpr) {
			*npp = npr->dprn_next;
			dt_free(dtp, npr);
		} else {
			npp = &npr->dprn_next;
		}
	}

	(void) pthread_mutex_unlock(&dph->dph_lock);

	/*
	 * Remove the dt_proc_list from the LRU list, release the underlying
	 * libproc handle, and free our dt_proc_t data structure.
	 */
	if (dpr->dpr_cacheable) {
		assert(dph->dph_lrucnt != 0);
		dph->dph_lrucnt--;
	}

	dt_list_delete(&dph->dph_lrulist, dpr);
	Prelease(dpr->dpr_proc, kill_it);
	dt_free(dtp, dpr);
}

static int
dt_proc_create_thread(dtrace_hdl_t *dtp, dt_proc_t *dpr, uint_t stop)
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
#if defined(sun)
	(void) sigdelset(&nset, SIGCANCEL);	/* see dt_proc_destroy() */
#else
	(void) sigdelset(&nset, SIGUSR1);	 /* see dt_proc_destroy() */
#endif

	data.dpcd_hdl = dtp;
	data.dpcd_proc = dpr;

	(void) pthread_sigmask(SIG_SETMASK, &nset, &oset);
	err = pthread_create(&dpr->dpr_tid, &a, dt_proc_control, &data);
	(void) pthread_sigmask(SIG_SETMASK, &oset, NULL);

	/*
	 * If the control thread was created, then wait on dpr_cv for either
	 * dpr_done to be set (the victim died or the control thread failed)
	 * or DT_PROC_STOP_IDLE to be set, indicating that the victim is now
	 * stopped by /proc and the control thread is at the rendezvous event.
	 * On success, we return with the process and control thread stopped:
	 * the caller can then apply dt_proc_continue() to resume both.
	 */
	if (err == 0) {
		while (!dpr->dpr_done && !(dpr->dpr_stop & DT_PROC_STOP_IDLE)) 
			(void) pthread_cond_wait(&dpr->dpr_cv, &dpr->dpr_lock);

		/*
		 * If dpr_done is set, the control thread aborted before it
		 * reached the rendezvous event.  This is either due to PS_LOST
		 * or PS_UNDEAD (i.e. the process died).  We try to provide a
		 * small amount of useful information to help figure it out.
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

	return (err);
}

struct ps_prochandle *
dt_proc_create(dtrace_hdl_t *dtp, const char *file, char *const *argv)
{
	dt_proc_hash_t *dph = dtp->dt_procs;
	dt_proc_t *dpr;
	int err;

	if ((dpr = dt_zalloc(dtp, sizeof (dt_proc_t))) == NULL)
		return (NULL); /* errno is set for us */

	(void) pthread_mutex_init(&dpr->dpr_lock, NULL);
	(void) pthread_cond_init(&dpr->dpr_cv, NULL);

	if ((dpr->dpr_proc = Pcreate(file, argv, &err, NULL, 0)) == NULL) {
		return (dt_proc_error(dtp, dpr,
		    "failed to execute %s: %s\n", file, Pcreate_error(err)));
	}

	dpr->dpr_hdl = dtp;
	dpr->dpr_pid = ps_getpid(dpr->dpr_proc);
	dpr->dpr_created = B_TRUE;

	if (dt_proc_create_thread(dtp, dpr, DT_PROC_STOP_IDLE) != 0) 
		return (NULL); /* dt_proc_error() has been called for us */
	dpr->dpr_hash = dph->dph_hash[dpr->dpr_pid & (dph->dph_hashlen - 1)];
	dph->dph_hash[dpr->dpr_pid & (dph->dph_hashlen - 1)] = dpr;
	dt_list_prepend(&dph->dph_lrulist, dpr);

	dt_dprintf("created pid %d\n", (int)dpr->dpr_pid);
	dpr->dpr_refs++;

	return (dpr->dpr_proc);
}

struct ps_prochandle *
dt_proc_grab(dtrace_hdl_t *dtp, pid_t pid, int flags, int nomonitor)
{
	dt_proc_hash_t *dph = dtp->dt_procs;
	uint_t h = pid & (dph->dph_hashlen - 1);
	dt_proc_t *dpr, *opr;
	int err;

	/*
	 * Search the hash table for the pid.  If it is already grabbed or
	 * created, move the handle to the front of the lrulist, increment
	 * the reference count, and return the existing ps_prochandle.
	 */
	for (dpr = dph->dph_hash[h]; dpr != NULL; dpr = dpr->dpr_hash) {
		if (dpr->dpr_pid == pid && !dpr->dpr_stale) {
			/*
			 * If the cached handle was opened read-only and
			 * this request is for a writeable handle, mark
			 * the cached handle as stale and open a new handle.
			 * Since it's stale, unmark it as cacheable.
			 */
			if (dpr->dpr_rdonly && !(flags & PGRAB_RDONLY)) {
				dt_dprintf("upgrading pid %d\n", (int)pid);
				dpr->dpr_stale = B_TRUE;
				dpr->dpr_cacheable = B_FALSE;
				dph->dph_lrucnt--;
				break;
			}

			dt_dprintf("grabbed pid %d (cached)\n", (int)pid);
			dt_list_delete(&dph->dph_lrulist, dpr);
			dt_list_prepend(&dph->dph_lrulist, dpr);
			dpr->dpr_refs++;
			return (dpr->dpr_proc);
		}
	}

	if ((dpr = dt_zalloc(dtp, sizeof (dt_proc_t))) == NULL)
		return (NULL); /* errno is set for us */

	(void) pthread_mutex_init(&dpr->dpr_lock, NULL);
	(void) pthread_cond_init(&dpr->dpr_cv, NULL);

	if ((dpr->dpr_proc = Pgrab(pid, flags, &err)) == NULL) {
		return (dt_proc_error(dtp, dpr,
		    "failed to grab pid %d: %s\n", (int)pid, Pgrab_error(err)));
	}

	dpr->dpr_hdl = dtp;
	dpr->dpr_pid = pid;
	dpr->dpr_created = B_FALSE;

	/*
	 * If we are attempting to grab the process without a monitor
	 * thread, then mark the process cacheable only if it's being
	 * grabbed read-only.  If we're currently caching more process
	 * handles than dph_lrulim permits, attempt to find the
	 * least-recently-used handle that is currently unreferenced and
	 * release it from the cache.  Otherwise we are grabbing the process
	 * for control: create a control thread for this process and store
	 * its ID in dpr->dpr_tid.
	 */
	if (nomonitor || (flags & PGRAB_RDONLY)) {
		if (dph->dph_lrucnt >= dph->dph_lrulim) {
			for (opr = dt_list_prev(&dph->dph_lrulist);
			    opr != NULL; opr = dt_list_prev(opr)) {
				if (opr->dpr_cacheable && opr->dpr_refs == 0) {
					dt_proc_destroy(dtp, opr->dpr_proc);
					break;
				}
			}
		}

		if (flags & PGRAB_RDONLY) {
			dpr->dpr_cacheable = B_TRUE;
			dpr->dpr_rdonly = B_TRUE;
			dph->dph_lrucnt++;
		}

	} else if (dt_proc_create_thread(dtp, dpr, DT_PROC_STOP_GRAB) != 0)
		return (NULL); /* dt_proc_error() has been called for us */

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
	    (!dpr->dpr_cacheable || dph->dph_lrucnt > dph->dph_lrulim))
		dt_proc_destroy(dtp, P);
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

	while ((dpr = dt_list_next(&dph->dph_lrulist)) != NULL)
		dt_proc_destroy(dtp, dpr->dpr_proc);

	dtp->dt_procs = NULL;
	dt_free(dtp, dph);
}

struct ps_prochandle *
dtrace_proc_create(dtrace_hdl_t *dtp, const char *file, char *const *argv)
{
	dt_ident_t *idp = dt_idhash_lookup(dtp->dt_macros, "target");
	struct ps_prochandle *P = dt_proc_create(dtp, file, argv);

	if (P != NULL && idp != NULL && idp->di_id == 0)
		idp->di_id = ps_getpid(P); /* $target = created pid */

	return (P);
}

struct ps_prochandle *
dtrace_proc_grab(dtrace_hdl_t *dtp, pid_t pid, int flags)
{
	dt_ident_t *idp = dt_idhash_lookup(dtp->dt_macros, "target");
	struct ps_prochandle *P = dt_proc_grab(dtp, pid, flags, 0);

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
