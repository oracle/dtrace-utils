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
 * Copyright 2010 -- 2015 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * DTrace Process Control
 *
 * This library provides several mechanisms in the libproc control layer:
 *
 * Process Control: a control thread is created for each process to provide
 * callbacks on process exit, to handle ptrace()-related signal dispatch tasks,
 * and to provide a central point that all ptrace()-related requests from the
 * rest of DTrace can flow through, working around the limitation that ptrace()
 * is per-thread and that libproc makes extensive use of it.
 *
 * MT-Safety: due to the above ptrace() limitations, libproc is not MT-Safe or
 * even capable of multithreading, so a marshalling and proxying layer is
 * provided to route all communication with libproc through the control thread.
 *
 * NOTE: MT-Safety is NOT provided for libdtrace itself, or for use of the
 * dtrace_proc_grab/dtrace_proc_create mechanisms.  Like all exported libdtrace
 * calls, these are assumed to be MT-Unsafe.  MT-Safety is ONLY provided for
 * calls via the libproc marshalling layer.  All calls from the rest of DTrace
 * must go via the dt_P*() functions, which in addition to routing calls via the
 * proxying layer also arrange to automatically retry in the event of child
 * exec().
 *
 * The ps_prochandles themselves are maintained along with a dt_proc_t struct in
 * a hash table indexed by PID.  This provides basic locking and reference
 * counting.  The dt_proc_t is also maintained in LRU order on dph_lrulist.  The
 * dph_lrucnt and dph_lrulim count the number of processes we have grabbed or
 * created but not retired, and the current limit on the number of actively
 * cached entries.
 *
 * The control threads currently invoke processes, resume them when
 * dt_ps_proc_continue() is called, manage ptrace()-related signal dispatch and
 * breakpoint handling tasks, handle libproc requests from the rest of DTrace
 * relating to their specific process, and notify interested parties when the
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
#include <setjmp.h>

#include <mutex.h>

#include <libproc.h>
#include <dt_proc.h>
#include <dt_pid.h>
#include <dt_impl.h>

enum dt_attach_time_t { ATTACH_START, ATTACH_ENTRY, ATTACH_FIRST_ARG_MAIN,
			ATTACH_DIRECT_MAIN };

static int dt_proc_attach_break(dt_proc_t *dpr, enum dt_attach_time_t attach_time);
static int dt_proc_reattach(dtrace_hdl_t *dtp, dt_proc_t *dpr);
static int dt_proc_monitor(dt_proc_t *dpr, int monitor);
static void dt_proc_scan(dtrace_hdl_t *dtp, dt_proc_t *dpr);
static int dt_proc_loop(dt_proc_t *dpr, int awaiting_continue);
static void dt_main_fail_rendezvous(dt_proc_t *dpr);
static void dt_proc_dpr_lock(dt_proc_t *dpr);
static void dt_proc_dpr_unlock(dt_proc_t *dpr);
static void dt_proc_ptrace_lock(struct ps_prochandle *P, void *arg,
    int ptracing);
static long dt_ps_proc_continue(dtrace_hdl_t *dtp, struct ps_prochandle *P);

/*
 * Locking assertions.
 */
#define assert_self_locked(dpr)			\
	do { \
		assert(MUTEX_HELD(&dpr->dpr_lock));		\
		assert(pthread_equal(dpr->dpr_lock_holder, pthread_self())); \
	} while(0)						\

/*
 * Unwinder pad for libproc setjmp() chains.
 */
static __thread jmp_buf *unwinder_pad;

static jmp_buf **
dt_unwinder_pad(struct ps_prochandle *unused)
{
	return &unwinder_pad;
}

static void
dt_proc_notify(dtrace_hdl_t *dtp, dt_proc_hash_t *dph, dt_proc_t *dpr,
    const char *msg, int lock, int broadcast)
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

		if (lock)
			(void) pthread_mutex_lock(&dph->dph_lock);

		dprn->dprn_next = dph->dph_notify;
		dph->dph_notify = dprn;

		if (broadcast)
			(void) pthread_cond_broadcast(&dph->dph_cv);
		if (lock)
			(void) pthread_mutex_unlock(&dph->dph_lock);
	}
}

/*
 * Wait for all processes in the dtp's hash table that are marked as dpr_created
 * but have no controlling thread. (These processes are counted in the dph, so
 * in the common case that there are none, this function can exit early and do
 * nothing.)
 *
 * Must be called under the dph_lock by the function that scans the notification
 * table, since no notifications are sent, only enqueued.
 */
void
dt_proc_enqueue_exits(dtrace_hdl_t *dtp)
{
	dt_proc_hash_t *dph = dtp->dt_procs;
	dt_proc_t *dpr;

	if (!dph->dph_noninvasive_created)
		return;

	assert(MUTEX_HELD(&dph->dph_lock));
	/*
	 * We can cheat here and save time by not taking out the
	 * dt_proc_dpr_lock, because in the case we are interested in there is
	 * no control thread to lock against, and in the only other case where
	 * dpr_done can end up set and dpr_tid be NULL, the control thread has
	 * died and the process has already been waited for.
	 *
	 * We also know that the process cannot be ptrace()d in this case, so
	 * there is no danger of a return indicating a trace stop.
	 */
	for (dpr = dt_list_next(&dph->dph_lrulist);
	     dpr != NULL; dpr = dt_list_next(dpr)) {
		if (dpr->dpr_tid == 0 && dpr->dpr_done && dpr->dpr_proc) {
			int exited = 0;
			siginfo_t info;

			info.si_pid = 0;
			/*
			 * We treat -ECHILD like a process exit, and consider
			 * other errors a real problem.
			 */
			if (waitid(P_PID, Pgetpid(dpr->dpr_proc), &info,
				WNOHANG | WEXITED) < 0) {
				if (errno != -ECHILD)
					dt_dprintf("Error waitid()ing for child "
					    "%i: %s\n", Pgetpid(dpr->dpr_proc),
					    strerror(errno));
				else
					exited = 1;
			}

			if (info.si_pid != 0)
				exited = 1;

			if (exited) {
				dt_proc_notify(dtp, dph, dpr, NULL,
				    B_FALSE, B_FALSE);
				Prelease(dpr->dpr_proc,
				    dpr->dpr_created ? PS_RELEASE_KILL :
				    PS_RELEASE_NORMAL);
				dph->dph_noninvasive_created--;
			}
		}
	}
}

/*
 * Check to see if the control thread was requested to stop when the victim
 * process reached a particular event (why) rather than continuing the victim.
 * If 'why' is set in the stop mask, we wait on dpr_cv for dt_ps_proc_continue().
 * If 'why' is not set, this function returns immediately and does nothing.
 */
static void
dt_proc_stop(dt_proc_t *dpr, uint8_t why)
{
	assert(MUTEX_HELD(&dpr->dpr_lock));
	assert(pthread_equal(dpr->dpr_lock_holder, pthread_self()));
	assert(pthread_equal(dpr->dpr_tid, pthread_self()));
	assert(why != DT_PROC_STOP_IDLE);

	if (dpr->dpr_stop & why) {
		unsigned long lock_count;

		dpr->dpr_stop |= DT_PROC_STOP_IDLE;
		dpr->dpr_stop &= ~why;

		pthread_cond_broadcast(&dpr->dpr_cv);

		/*
		 * Exit out of all but one lock, so the unlock in dt_proc_loop()
		 * unlocks us all the way, and proxy requests can get in.
		 *
		 * Then wait for proxy requests, but not process state changes.
		 * (Even though we are not waiting for process state changes, we
		 * may nonetheless be informed of some, notably process death
		 * and execve(), via other routes inside libproc.)
		 */
		lock_count = dpr->dpr_lock_count_ctrl;
		dpr->dpr_lock_count_ctrl = 1;

		if (dt_proc_loop(dpr, 1) < 0) {
			/*
			 * The process has died. Just return.
			 */
			return;
		}

		dpr->dpr_lock_count_ctrl = lock_count;
		dpr->dpr_stop |= DT_PROC_STOP_RESUMING;

		dt_dprintf("%d: dt_proc_stop(), control thread now waiting "
		    "for resume.\n", (int) dpr->dpr_pid);
	}
}

/*
 * After a stop is carried out and we have carried out any operations that must
 * be done serially, we must signal back to the process waiting in
 * dt_ps_proc_continue() that it can resume.
 */
static void
dt_proc_resume(dt_proc_t *dpr)
{
	assert(MUTEX_HELD(&dpr->dpr_lock));
	assert(pthread_equal(dpr->dpr_lock_holder, pthread_self()));

	dt_dprintf("dt_proc_resume(), dpr_stop: 0x%x (%i)\n",
	    dpr->dpr_stop, dpr->dpr_stop & DT_PROC_STOP_RESUMING);

	if (dpr->dpr_stop & DT_PROC_STOP_RESUMING) {
		dpr->dpr_stop &= ~DT_PROC_STOP_RESUMING;
		dpr->dpr_stop |= DT_PROC_STOP_RESUMED;
		pthread_cond_broadcast(&dpr->dpr_cv);

		dt_dprintf("dt_proc_resume(), control thread resumed. "
		    "Lock count: %lu\n", dpr->dpr_lock_count_ctrl);
	}
}

/*
 * Fire a one-shot breakpoint to say that the child has got to an interesting
 * place from which we should grab control, possibly blocking.
 *
 * The dpr_lock is already held when this function is called.
 */
static int
dt_break_interesting(uintptr_t addr, void *dpr_data)
{
	dt_proc_t *dpr = dpr_data;

	dt_dprintf("pid %d: breakpoint on interesting locus\n",
	    (int)dpr->dpr_pid);

	Punbkpt(dpr->dpr_proc, addr);
	dt_proc_scan(dpr->dpr_hdl, dpr);
	dt_proc_stop(dpr, dpr->dpr_hdl->dt_prcmode);
	dt_proc_resume(dpr);

	return PS_RUN;
}

/*
 * A one-shot breakpoint that fires at a point at which the dynamic linker has
 * initialized far enough to enable us to do reliable symbol lookups, and thus
 * drop a breakpoint on a function.  The function we drop it on is
 * __libc_start_main(), in libc, which takes the address of main() as its first
 * argument.  Statically linked programs don't have this function, but might
 * have an exported main() we can look up directly, or might have nothing, in
 * which case we resume immediately, just as if evaltime=preinit were used.
 *
 * The dpr_lock is already held when this function is called.
 */
static int
dt_break_prepare_drop_main(uintptr_t addr, void *dpr_data)
{
	dt_proc_t *dpr = dpr_data;
	int ret = -1;

	dt_dprintf("pid %d: breakpoint on process start\n",
	    (int)dpr->dpr_pid);

	Punbkpt(dpr->dpr_proc, addr);

	/*
	 * Dynamically linked: scan for shared libraries, and drop a breakpoint
	 * on __libc_start_main().
	 */
	if (Pdynamically_linked(dpr->dpr_proc) > 0) {
		dt_proc_scan(dpr->dpr_hdl, dpr);
		ret = dt_proc_attach_break(dpr, ATTACH_ENTRY);
	}

	/*
	 * If statically linked, or if for whatever reason we couldn't find
	 * __libc_start_main(), just try dropping a breakpoint on main(),
	 * instead.
	 */

	if (ret < 0)
		ret = dt_proc_attach_break(dpr, ATTACH_DIRECT_MAIN);

	if (ret < 0)
		dt_main_fail_rendezvous(dpr);

	return PS_RUN;
}

/*
 * A one-shot breakpoint that fires at the start of __libc_start_main(), the
 * libc function which is the immediate parent of main() in the call stack.
 *
 * It is passed the address of main() as its first (pointer) argument.
 */
static int
dt_break_libc_start_main(uintptr_t addr, void *dpr_data)
{
	dt_proc_t *dpr = dpr_data;
	int ret = -1;

	dt_dprintf("pid %d: breakpoint on __libc_start_main()\n",
	    (int)dpr->dpr_pid);

	Punbkpt(dpr->dpr_proc, addr);

	ret = dt_proc_attach_break(dpr, ATTACH_FIRST_ARG_MAIN);

	/*
	 * Failed. Just try dropping a breakpoint on main(), instead.
	 */
	if (ret < 0)
		ret = dt_proc_attach_break(dpr, ATTACH_DIRECT_MAIN);

	if (ret < 0)
		dt_main_fail_rendezvous(dpr);

	return PS_RUN;
}

/*
 * If we couldn't dt_proc_attach_break(), because we couldn't find main() in any
 * fashion, rendezvous here, instead.
 */
static void
dt_main_fail_rendezvous(dt_proc_t *dpr)
{
	dt_dprintf("pid %d: main() lookup failed, resuming now\n", (int)dpr->dpr_pid);
	dt_proc_stop(dpr, dpr->dpr_hdl->dt_prcmode);
	dt_proc_resume(dpr);
}

/*
 * New shared libraries seen: update our idea of the process's state
 * accordingly.
 */
static void
dt_proc_scan(dtrace_hdl_t *dtp, dt_proc_t *dpr)
{
	Pupdate_syms(dpr->dpr_proc);
	if (dt_pid_create_probes_module(dtp, dpr) != 0)
		dt_proc_notify(dtp, dtp->dt_procs, dpr, dpr->dpr_errmsg,
		    B_TRUE, B_TRUE);
}

/*
 * Event handler invoked automatically from within Pwait() when an interesting
 * event occurs.
 *
 * The dpr_lock is already held when this function is called.
 */

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

	/* cannot happen, but do nothing anyway */
	if (msg->type == RD_NONE)
		return;

	/*
	 * Call dt_proc_scan() on the first consistent report after
	 * an add or remove.
	 */
	switch (msg->state) {
	case RD_ADD:
	case RD_DELETE:
		dpr->dpr_awaiting_dlactivity = 1;
		break;
	case RD_CONSISTENT:
		if (dpr->dpr_awaiting_dlactivity) {
			dt_proc_scan(dtp, dpr);
			dpr->dpr_awaiting_dlactivity = 0;
		}
	}
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
 * This may be called twice, before the dt_ps_proc_continue() rendezvous just in
 * case the dynamic linker is far enough up to help us out, and from a
 * breakpoint set on preinit otherwise.
 *
 * Returns 0 on success, or -1 on failure (in which case the process is
 * still halted).
 */
static int
dt_proc_attach_break(dt_proc_t *dpr, enum dt_attach_time_t attach_time)
{
	uintptr_t addr = 0;
	GElf_Sym sym;
	dtrace_hdl_t *dtp = dpr->dpr_hdl;
	int (*handler) (uintptr_t addr, void *data) = dt_break_interesting;

	assert(MUTEX_HELD(&dpr->dpr_lock));

	dt_proc_rdagent(dpr);

	dt_dprintf("Called dt_attach() with attach_time %i\n", attach_time);

	/*
	 * If we're stopping on exec we have no breakpoints to drop: if
	 * we're stopping on preinit and it's after the dt_ps_proc_continue()
	 * rendezvous, we've already dropped the necessary breakpoints.
	 */

	if (dtp->dt_prcmode == DT_PROC_STOP_CREATE)
		return 0;

	if (attach_time != ATTACH_START &&
	    dtp->dt_prcmode == DT_PROC_STOP_PREINIT)
		return 0;

	switch (attach_time) {
	case ATTACH_START:
		/*
		 * Before dt_ps_proc_continue().  Preinit, postinit and main all
		 * get a breakpoint dropped on the process entry point, though
		 * postinit and main use a different handler.
		 */
		switch (dtp->dt_prcmode) {
		case DT_PROC_STOP_POSTINIT:
		case DT_PROC_STOP_MAIN:
			handler = dt_break_prepare_drop_main;
		case DT_PROC_STOP_PREINIT:
			dt_dprintf("pid %d: dropping breakpoint on AT_ENTRY\n",
			    (int)dpr->dpr_pid);
			addr = Pgetauxval(dpr->dpr_proc, AT_ENTRY);
		}
		break;
	case ATTACH_ENTRY:
		/*
		 * Stopped at the process entry point.  Drop a breakpoint on
		 * __libc_start_main().  If we can't, immediately return an
		 * error: we may be called again with a request to use a
		 * different approach to find main().
		 */
		dt_dprintf("pid %d: dropping breakpoint on __libc_start_main\n",
		    (int)dpr->dpr_pid);

		handler = dt_break_libc_start_main;
		if (Pxlookup_by_name(dpr->dpr_proc, LM_ID_BASE,
			PR_OBJ_EVERY, "__libc_start_main", &sym, NULL) == 0)
			addr = sym.st_value;
		else {
			dt_dprintf("pid %d: cannot resolve __libc_start_main\n",
			    (int)dpr->dpr_pid);
			return -1;
		}
		break;
	case ATTACH_FIRST_ARG_MAIN:
		/*
		 * After dt_ps_proc_continue(), stopped at __libc_start_main().
		 * main()s address is passed as the first argument to this
		 * function.
		 */
		dt_dprintf("pid %d: dropping breakpoint on address of "
		    "__libc_start_main's first arg\n", (int)dpr->dpr_pid);

		addr = Pread_first_arg(dpr->dpr_proc);
		if (addr == (uintptr_t) -1) {
			dt_dprintf("Cannot look up __libc_start_main()'s "
			    "first arg: %s\n", strerror(errno));
			return -1;
		}
		break;
	case ATTACH_DIRECT_MAIN:
		/*
		 * After dt_ps_proc_continue().  Drop a breakpoint on main(),
		 * via a normal symbol lookup.
		 */
		dt_dprintf("pid %d: dropping breakpoint on main() by symbol "
		    "lookup\n", (int)dpr->dpr_pid);

		if (Pxlookup_by_name(dpr->dpr_proc, LM_ID_BASE,
			PR_OBJ_EVERY, "main", &sym, NULL) == 0)
			addr = sym.st_value;
		break;
	}

	if (addr &&
	    Pbkpt(dpr->dpr_proc, addr, B_FALSE, handler, NULL, dpr) == 0)
		return 0;

	/*
	 * This statement is not quite accurate: there is no way to simulate the
	 * effect of a DTrace that resumes tracing at e.g. __libc_start_main()
	 * via any evaltime option.  But it's nearly right.
	 */
	dt_dprintf("Cannot drop breakpoint in child process: acting as if "
	    "evaltime=%s were in force.\n", attach_time == ATTACH_START ?
	    "exec" : "preinit");

	/*
	 * Arrange to stall DTrace until either the creation rendezvous (if this
	 * is the first attachment) or until the preinit rendezvous (an
	 * arbitrary later rendezvous point reached when the eventual breakpoint
	 * on main() or wherever is finally reached).
	 */
	dpr->dpr_stop &= ~dtp->dt_prcmode;
	if (attach_time == ATTACH_START) {
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
	va_list tmp;

	va_start(ap, format);
	va_copy(tmp, ap);
	dt_set_errmsg(dtp, NULL, NULL, NULL, 0, format, ap);
	va_end(ap);
	dt_debug_printf("dt_proc_error", format, tmp);
	va_end(tmp);

	(void) dt_set_errno(dtp, EDT_COMPILER);
	return (NULL);
}

/*
 * Proxy requests, routed via the control thread.
 *
 * Must be called under dpr_lock.
 *
 * Optionally, triggers a longjmp() to the exec-handler pad if an exec() is
 * detected in the child.  (Not all calls trigger this, because not all calls to
 * the control thread are related to the child process, and because some calls
 * to the child process are themselves involved in the implementation of the
 * exec-retry protocol.)
 */

static long
proxy_call(dt_proc_t *dpr, long (*proxy_rq)(), int exec_retry)
{
	char junk = '\0'; /* unimportant */

	dpr->dpr_proxy_rq = proxy_rq;

	if (dpr->dpr_done) {
		dt_proc_error(dpr->dpr_hdl, dpr, "Cannot write to proxy pipe, "
		    "control thread is dead\n");
		return(-1);
	}

	errno = 0;
	while (write(dpr->dpr_proxy_fd[1], &junk, 1) < 0 && errno == EINTR);
	if (errno != 0 && errno != EINTR) {
		dt_proc_error(dpr->dpr_hdl, dpr, "Cannot write to proxy pipe "
		    "for Pwait(), deadlock is certain: %s\n", strerror(errno));
		return (-1);
	}

	while (dpr->dpr_proxy_rq != NULL)
		pthread_cond_wait(&dpr->dpr_msg_cv, &dpr->dpr_lock);

	dpr->dpr_lock_holder = pthread_self();

	if (exec_retry && dpr->dpr_proxy_exec_retry && *unwinder_pad)
		longjmp(*unwinder_pad, dpr->dpr_proxy_exec_retry);

	errno = dpr->dpr_proxy_errno;
	return dpr->dpr_proxy_ret;
}

static long
proxy_pwait(struct ps_prochandle *P, void *arg, boolean_t block)
{
	dt_proc_t *dpr = arg;

	assert_self_locked(dpr);

	/*
	 * If we are already in the right thread, pass the call straight on.
	 */
	if (pthread_equal(dpr->dpr_tid, pthread_self()))
		return Pwait_internal(P, block);

	dpr->dpr_proxy_args.dpr_pwait.P = P;
	dpr->dpr_proxy_args.dpr_pwait.block = block;

	return proxy_call(dpr, proxy_pwait, 1);
}

static long
proxy_ptrace(enum __ptrace_request request, void *arg, pid_t pid, void *addr,
    void *data)
{
	dt_proc_t *dpr = arg;

	assert_self_locked(dpr);

	/*
	 * If we are already in the right thread, pass the call
	 * straight on.
	 */
	if (pthread_equal(dpr->dpr_tid, pthread_self()))
		return ptrace(request, pid, addr, data);

	dpr->dpr_proxy_args.dpr_ptrace.request = request;
	dpr->dpr_proxy_args.dpr_ptrace.pid = pid;
	dpr->dpr_proxy_args.dpr_ptrace.addr = addr;
	dpr->dpr_proxy_args.dpr_ptrace.data = data;

	return proxy_call(dpr, proxy_ptrace, 1);
}

/*
 * This proxy request serves to force the controlling thread to recreate its
 * ps_prochandle after an exec().
 */
static long
proxy_reattach(dt_proc_t *dpr)
{
	assert_self_locked(dpr);

	/*
	 * If we are already in the right thread, pass the call straight on.
	 */
	if (pthread_equal(dpr->dpr_tid, pthread_self()))
		return dt_proc_reattach(dpr->dpr_hdl, dpr);

	return proxy_call(dpr, proxy_reattach, 0);
}

/*
 * This proxy request requests that the controlling thread cease background
 * monitoring of the process: monitor requests will arrive via proxy Pwait()
 * calls.  It is also used to re-enable said monitoring.
 *
 * This request cannot trigger an exec-retry, as it does not monitor process
 * state changes.
 */
static long
proxy_monitor(dt_proc_t *dpr, int monitor)
{
	assert_self_locked(dpr);

	/*
	 * If we are already in the right thread, pass the call straight on.
	 */
	if (pthread_equal(dpr->dpr_tid, pthread_self()))
		return dt_proc_monitor(dpr, monitor);

	dpr->dpr_proxy_args.dpr_monitor.monitor = monitor;

	return proxy_call(dpr, proxy_monitor, 0);
}

typedef struct dt_proc_control_data {
	dtrace_hdl_t *dpcd_hdl;			/* DTrace handle */
	dt_proc_t *dpcd_proc;			/* process to control */
	int dpcd_flags;

	/*
	 * This pipe contains data only when the dt_proc.proxy_rq contains a
	 * proxy request that needs handling on behalf of DTrace's main thread.
	 * DTrace will be waiting for the response on the dpr_msg_cv.
	 */
	int dpcd_proxy_fd[2];

	/*
	 * The next two are only valid while the master thread is calling
	 * dt_ps_proc_create(), and only useful when dpr_created is true.
	 */
	const char *dpcd_start_proc;
	char * const *dpcd_start_proc_argv;
} dt_proc_control_data_t;

static void dt_proc_control_cleanup(void *arg);

/*
 * Entry point for all victim process control threads.	We initialize all the
 * appropriate /proc control mechanisms, start the process and halt it, notify
 * the caller of this, then wait for the caller to indicate its readiness and
 * resume the process: only then do we enter the main control loop (above).  We
 * exit when the victim dies.
 *
 * The control thread synchronizes the use of dpr_proc with other libdtrace
 * threads using dpr_lock.  We hold the lock for all of our operations except
 * waiting while the process is running.  If the libdtrace client wishes to exit
 * or abort our wait, thread cancellation can be used.
 */
static void *
dt_proc_control(void *arg)
{
	dt_proc_control_data_t * volatile datap = arg;
	dtrace_hdl_t * volatile dtp = datap->dpcd_hdl;
	dt_proc_t * volatile dpr = datap->dpcd_proc;
	int err;
	jmp_buf exec_jmp;

	/*
	 * Set up global libproc hooks that must be active before any processes
	 * are * grabbed or created.
	 */
	Pset_ptrace_lock_hook(dt_proc_ptrace_lock);
	Pset_libproc_unwinder_pad(dt_unwinder_pad);

	/*
	 * Arrange to clean up when cancelled by dt_ps_proc_destroy() on shutdown.
	 */
	pthread_cleanup_push(dt_proc_control_cleanup, dpr);

	/*
	 * Lock our mutex, preventing races between cv broadcasts to our
	 * controlling thread and dt_ps_proc_continue() or process destruction.
	 *
	 * It is eventually unlocked by dt_proc_control_cleanup(), and
	 * temporarily unlocked (while waiting) by dt_proc_loop().
	 */
	dt_proc_dpr_lock(dpr);

	dpr->dpr_proxy_fd[0] = datap->dpcd_proxy_fd[0];
	dpr->dpr_proxy_fd[1] = datap->dpcd_proxy_fd[1];

	/*
	 * Either create the process, or grab it.  Whichever, on failure, quit
	 * and let our cleanup run (signalling failure to
	 * dt_ps_proc_create_thread() in the process).
	 *
	 * At this point, the process is halted at exec(), if created.
	 */

	if (dpr->dpr_created) {
		if ((dpr->dpr_proc = Pcreate(datap->dpcd_start_proc,
			    datap->dpcd_start_proc_argv, dpr, &err)) == NULL) {
			dt_proc_error(dtp, dpr, "failed to execute %s: %s\n",
			    datap->dpcd_start_proc,
			    strerror(err));
			pthread_exit(NULL);
		}
		dpr->dpr_pid = Pgetpid(dpr->dpr_proc);
	} else {
		int noninvasive = 0;

		/*
		 * "Shortlived" means that the monitoring of this process is not
		 * especially important: that it is one of many processes being
		 * grabbed by something like a mass u*() action.  It might still
		 * be worth ptracing it so that we get better symbol resolution,
		 * but if the process is a crucial system daemon, avoid ptracing
		 * it entirely, to avoid rtld_db dropping breakpoints in crucial
		 * system daemons unless specifically demanded.  No death
		 * notification is ever sent.
		 *
		 * Also, obviously enough, never drop breakpoints in ourself!
		 */
		if (datap->dpcd_flags & DTRACE_PROC_SHORTLIVED) {
			noninvasive = 1;
			dpr->dpr_notifiable = 0;
			if ((Psystem_daemon(dpr->dpr_pid, dtp->dt_useruid,
				    dtp->dt_sysslice) > 0) ||
			    (dpr->dpr_pid == getpid()))
				noninvasive = 2;
		}

		if ((dpr->dpr_proc = Pgrab(dpr->dpr_pid, noninvasive, 0,
			    dpr, &err)) == NULL) {
			dt_proc_error(dtp, dpr, "failed to grab pid %li: %s\n",
			    (long) dpr->dpr_pid, strerror(err));
			pthread_exit(NULL);
		}

		/*
		 * If this was a noninvasive grab, quietly exit without calling
		 * the cleanup handlers: the process is running, but does not
		 * need a monitoring thread.  Process termination detection is
		 * handled in the parent process, via the subreaper already set
		 * up.
		 */
		if (noninvasive && !Ptraceable(dpr->dpr_proc)) {
			dt_dprintf("%i: noninvasive grab, control thread "
			    "suiciding\n", dpr->dpr_pid);
			pthread_exit(NULL);
		}
	}

	/*
	 * Arrange to proxy Pwait() and ptrace() calls through the
	 * thread-spanning proxies.
	 */
	Pset_pwait_wrapper(dpr->dpr_proc, proxy_pwait);
	Pset_ptrace_wrapper(dpr->dpr_proc, proxy_ptrace);

	/*
	 * Make a waitfd to this process, and set up polling structures
	 * appropriately.  WEXITED | WSTOPPED is what Pwait() waits for.
	 */
	if ((dpr->dpr_fd = waitfd(P_PID, dpr->dpr_pid, WEXITED | WSTOPPED, 0)) < 0) {
		dt_proc_error(dtp, dpr, "failed to get waitfd() for pid %li: %s\n",
		    (long) dpr->dpr_pid, strerror(errno));
		/*
		 * Demote this to a mandatorily noninvasive grab: if we
		 * Pcreate()d it, dpr_created is still set, so it will still get
		 * killed on dtrace exit.  If even that fails, there's nothing
		 * we can do but hope.
		 *
		 * The dt_proc_exit_check() function, called by dtrace_sleep(),
		 * checks for termination of such processes (since nothing else
		 * will).
		 */
		Prelease(dpr->dpr_proc, PS_RELEASE_NORMAL);
		if ((dpr->dpr_proc = Pgrab(dpr->dpr_pid, 2, 0,
			    dpr, &err)) == NULL) {
			dt_proc_error(dtp, dpr, "failed to regrab pid %li: %s\n",
			    (long) dpr->dpr_pid, strerror(err));
		}

		dtp->dt_procs->dph_noninvasive_created++;
		pthread_exit(NULL);
	}

	/*
	 * Detect execve()s from loci in this thread other than proxy calls:
	 * handle them by destroying and re-grabbing the libproc handle without
	 * detaching or re-ptracing from it, then forcibly resetting the dpr
	 * lock count (we must hold the dpr lock at this point).  There can be
	 * no existing exec jmp_buf, so don't try to chain to one.  After this
	 * point, the process is stopped at exec() just as after a Pcreate().
	 */
	if (setjmp(exec_jmp)) {
		/*
		 * dt_proc_reattach() calls P*() functions which can rethrow.
		 * The unwinder-pad is not reset during the throw: we must
		 * reset it now so that such a rethrow will work.
		 */
		unwinder_pad = &exec_jmp;
		err = dt_proc_reattach(dtp, dpr);
		if (err != 0) {
			dt_proc_error(dtp, dpr,
			    "failed to regrab pid %li after exec(): %s\n",
			    (long) dpr->dpr_pid, strerror(err));
			pthread_exit(NULL);
		}
	} else {
		unwinder_pad = &exec_jmp;
		/*
		 * Enable rtld breakpoints at the location specified by
		 * dt_prcmode (or drop other breakpoints which will eventually
		 * enable us to drop breakpoints at that location).
		 */
		dt_proc_attach_break(dpr, ATTACH_START);

		/*
		 * Wait for a rendezvous from dt_ps_proc_continue(), iff we were
		 * called under DT_PROC_STOP_CREATE or DT_PROC_STOP_GRAB.  After
		 * this point, datap and all that it points to is no longer
		 * valid.
		 *
		 * This covers evaltime=exec and grabs, but not the three
		 * evaltimes that depend on breakpoints.  Those wait for
		 * rendezvous from within the breakpoint handler, invoked from
		 * Pwait() in dt_proc_loop().
		 */
		dt_proc_stop(dpr, dpr->dpr_created ? DT_PROC_STOP_CREATE :
		    DT_PROC_STOP_GRAB);

		/*
		 * Set the process going, if it was stopped by the call above.
		 */

		Ptrace_set_detached(dpr->dpr_proc, dpr->dpr_created);
		Puntrace(dpr->dpr_proc, 0);
	}

	/*
	 * Notify the main thread that it is now safe to return from
	 * dt_ps_proc_continue().  If the process exec()s after this point, this
	 * call is redundant, but harmless, and it saves setting up a new setjmp
	 * handler just to skip it.
	 *
	 * Then enter the main control loop.
	 */

	dt_proc_resume(dpr);
	dt_proc_loop(dpr, 0);

	/*
	 * If the caller is *still* waiting in dt_ps_proc_continue() (i.e. the
	 * monitored process died before dtracing could start), resume it; then
	 * clean up.
	 */
	dt_proc_resume(dpr);
	pthread_cleanup_pop(1);

	return (NULL);
}

/*
 * Main loop for all victim process control threads.  We wait for changes in
 * process state or incoming proxy or continue requests from DTrace proper, and
 * handle each of those accordingly.  We can be asked not to wait for the
 * process (because the caller knows it is halted), in which case we respond to
 * incoming continue requests by exiting so that the caller (which was waiting
 * for them) can do its work.  We also exit if the victim dies (returning -1).
 */
static int
dt_proc_loop(dt_proc_t *dpr, int awaiting_continue)
{
	volatile struct pollfd pfd[2];

	assert(MUTEX_HELD(&dpr->dpr_lock));

	/*
	 * We always want to listen on the proxy pipe; we only want to listen on
	 * the process's waitfd pipe sometimes.
	 */

	pfd[0].events = POLLIN;
	pfd[1].fd = dpr->dpr_proxy_fd[0];
	pfd[1].events = POLLIN;

	/*
	 * If we're only proxying while waiting for a dt_ps_proc_continue(),
	 * avoid waiting on the process's fd.
	 */
	if (awaiting_continue)
		pfd[0].fd = dpr->dpr_fd * -1;

	/*
	 * Wait for the process corresponding to this control thread to stop,
	 * process the event, and then set it running again.  We want to sleep
	 * with dpr_lock *unheld* so that other parts of libdtrace can send
	 * requests to us, which is protected by that lock.  It is impossible
	 * for them, or any thread but this one, to modify the Pstate(), so we
	 * can call that without grabbing the lock.
	 */
	for (;;) {
		volatile int did_proxy_pwait = 0;

		dt_proc_dpr_unlock(dpr);

		/*
		 * If we should stop monitoring the process and only listen for
		 * proxy requests, avoid waiting on its fd.
		 */

		if (!awaiting_continue) {
			if (!dpr->dpr_monitoring)
				pfd[0].fd = dpr->dpr_fd * -1;
			else
				pfd[0].fd = dpr->dpr_fd;
		}

		while (errno = EINTR,
		    poll((struct pollfd *) pfd, 2, -1) <= 0 && errno == EINTR)
			continue;

		/*
		 * We can block for arbitrarily long periods on this lock if the
		 * main thread is in a Ptrace()/Puntrace() region, unblocking
		 * only briefly when requests come in from the process.	 This
		 * will not introduce additional latencies because the process
		 * is generally halted at this point, and being frequently
		 * Pwait()ed on by libproc (which proxies back to here).
		 *
		 * Note that if the process state changes while the lock is
		 * taken out by the main thread, the main thread will often
		 * proceed to Pwait() on it.  The ordering of these next two
		 * block is therefore crucial: we must check for proxy requests
		 * from the main thread *before* we check for process state
		 * changes via Pwait(), because one of the proxy requests is a
		 * Pwait(), and the libproc in the main thread often wants to
		 * unblock only once that Pwait() has returned (possibly after
		 * running breakpoint handlers and the like, which will run in
		 * the control thread, with their effects visible in the main
		 * thread, all serialized by dpr_lock).
		 */
		dt_proc_dpr_lock(dpr);

		/*
		 * Incoming proxy request.  Drain this byte out of the pipe, and
		 * handle it, with a new jmp_buf set up so as to redirect
		 * execve() detections back the calling thread.  (Multiple bytes
		 * cannot land on the pipe, so we don't need to consider this
		 * case -- but if they do, it is harmless, because the
		 * dpr_proxy_rq will be NULL in subsequent calls.)
		 */
		if (pfd[1].revents != 0) {
			char junk;
			jmp_buf this_exec_jmp, *old_exec_jmp;
			volatile int did_exec_retry = 0;

			read(dpr->dpr_proxy_fd[0], &junk, 1);
			pfd[1].revents = 0;

			/*
			 * execve() detected during a proxy request: notify the
			 * calling thread.  Do not rejump: we want to keep
			 * looping, and the exec jmp_buf is in another thread's
			 * call stack at this point.
			 */
			old_exec_jmp = unwinder_pad;
			if (setjmp(this_exec_jmp)) {
				dpr->dpr_proxy_exec_retry = 1;
				pthread_cond_signal(&dpr->dpr_msg_cv);
				did_exec_retry = 1;
			} else {
				unwinder_pad = &this_exec_jmp;

				/*
				 * Pwait() from another thread.
				 */
				if (dpr->dpr_proxy_rq == proxy_pwait) {
					dt_dprintf("%d: Handling a proxy Pwait(%i)\n",
					    dpr->dpr_pid,
					    dpr->dpr_proxy_args.dpr_pwait.block);
					errno = 0;
					dpr->dpr_proxy_ret = proxy_pwait
					    (dpr->dpr_proxy_args.dpr_pwait.P, dpr,
						dpr->dpr_proxy_args.dpr_pwait.block);

					did_proxy_pwait = 1;
				/*
				 * Ptrace() from another thread.
				 */
				} else if (dpr->dpr_proxy_rq == proxy_ptrace) {
					dt_dprintf("%d: Handling a proxy ptrace()\n",
					    dpr->dpr_pid);
					errno = 0;
					dpr->dpr_proxy_ret = proxy_ptrace
					    (dpr->dpr_proxy_args.dpr_ptrace.request,
						dpr,
						dpr->dpr_proxy_args.dpr_ptrace.pid,
						dpr->dpr_proxy_args.dpr_ptrace.addr,
						dpr->dpr_proxy_args.dpr_ptrace.data);
				/*
				 * Other thread in dt_proc_continue().
				 */
				} else if (dpr->dpr_proxy_rq == dt_proc_continue) {
					dt_dprintf("%d: Handling a dt_proc_continue()\n",
					    dpr->dpr_pid);
					if (!awaiting_continue) {
						dt_dprintf("Not blocked waiting for a "
						    "continue: skipping.");
					} else {
						/*
						 * Return: let dt_proc_stop() handle
						 * everything else.
						 */
						dpr->dpr_proxy_rq = NULL;
						unwinder_pad = old_exec_jmp;
						return 0;
					}
					dpr->dpr_proxy_ret = 0;
				/*
				 * execve() detecte: the other thread requests
				 * that we reattach to the traced process, and
				 * set it going again.
				 */
				} else if (dpr->dpr_proxy_rq == proxy_reattach) {
					int err;

					dt_dprintf("%d: Handling a proxy_reattach()\n",
				 	    dpr->dpr_pid);
					errno = 0;
					dpr->dpr_proxy_ret = 0;
					err = dt_proc_reattach(dpr->dpr_hdl, dpr);
					if (err != 0) {
						dpr->dpr_proxy_errno = err;
						dpr->dpr_proxy_rq = NULL;
						dpr->dpr_proxy_ret = err;
						pthread_cond_signal(&dpr->dpr_msg_cv);
						pthread_exit(NULL);
					}
				/*
				 * Request to cease background process
				 * monitoring.
				 */
				} else if (dpr->dpr_proxy_rq == proxy_monitor) {
					dt_dprintf("%d: Handling a proxy_monitor(%i)\n",
				 	    dpr->dpr_pid, dpr->dpr_proxy_args.dpr_monitor.monitor);
					errno = 0;
					dpr->dpr_proxy_ret = proxy_monitor(dpr,
					    dpr->dpr_proxy_args.dpr_monitor.monitor);
				} else
					dt_dprintf("%d: unknown libproc request\n",
					    dpr->dpr_pid);
			}

			if (!did_exec_retry) {
				dpr->dpr_proxy_errno = errno;
				dpr->dpr_proxy_exec_retry = 0;
				dpr->dpr_proxy_rq = NULL;
				pthread_cond_signal(&dpr->dpr_msg_cv);
			}
			unwinder_pad = old_exec_jmp;
		}

		/*
		 * The process needs attention. Pwait() for it (which will make
		 * the waitfd transition back to empty).
		 */
		if (pfd[0].revents != 0) {
			dt_dprintf("%d: Handling a process state change\n",
			    dpr->dpr_pid);
			pfd[0].revents = 0;
			Pwait(dpr->dpr_proc, B_FALSE);

			switch (Pstate(dpr->dpr_proc)) {
			case PS_STOP:

				/*
				 * If the process stops showing one of the
				 * events that we are tracing, perform the
				 * appropriate response.
				 *
				 * TODO: the stop() action may need some work
				 * here.
				 */
				break;

				/*
				 * If the libdtrace caller (as opposed to any
				 * other process) tries to debug a monitored
				 * process, it may lead to our waitpid()
				 * returning strange results.  Fail in this
				 * case, until we define a protocol for
				 * communicating the waitpid() results to the
				 * caller, or relinquishing control temporarily.
				 * FIXME.
				 *
				 * Do not warn if we just did a proxy Pwait(),
				 * un which case we may well have detected an
				 * intentional transition to trace-stop.
				 */
			case PS_TRACESTOP:
				if (!did_proxy_pwait)
					dt_dprintf("%d: trace stop, nothing we "
					    "can do\n", dpr->dpr_pid);
				break;

			case PS_DEAD:
				dt_dprintf("%d: proc died\n", dpr->dpr_pid);
				return -1;
			}

			/*
			 * If we don't yet have an rtld_db handle, try again to
			 * get one.  (ld.so can take arbitrarily long to get
			 * ready for this.)
			 */
			dt_proc_rdagent(dpr);
		}
	}
}

/*
 * Cleanup handler, called when a process control thread exits or is cancelled.
 */
static void
dt_proc_control_cleanup(void *arg)
{
	int proc_existed = 0;
	int suiciding = 0;
	dt_proc_t *dpr = arg;

	/*
	 * Blank out the unwinder pad. Even if an exec() is detected at this
	 * point, we don't want to unwind back into the thread main().
	 */
	unwinder_pad = NULL;

	/*
	 * If the process is noninvasively traced, the control thread will
	 * suicide: we want to exit without reporting process death or releasing
	 * the libproc handle (since it is still in active use).
	 */
	if (dpr->dpr_proc && !Ptraceable(dpr->dpr_proc))
		suiciding = 1;

	/*
	 * Set dpr_done and clear dpr_tid to indicate that the control thread
	 * has exited, and notify any waiting thread on the dph_cv or in
	 * dt_ps_proc_destroy() that we have successfully exited.  Clean up the
	 * libproc state, unless this is a non-ptraceable process that doesn't
	 * need a monitor thread. (In that case, the monitor thread is suiciding
	 * but the libproc state should remain.)
	 *
	 * If we were cancelled while already holding the mutex, don't lock it
	 * again.
	 *
	 * Forcibly reset the lock count: we don't care about nested locks taken
	 * out by ptrace() wrappers above us in the call stack, since the whole
	 * thread is going away.
	 */
	dt_dprintf("%i: process control thread going away.\n", dpr->dpr_pid);
	if(dpr->dpr_lock_count_ctrl == 0 ||
	    !pthread_equal(dpr->dpr_lock_holder, pthread_self()))
		dt_proc_dpr_lock(dpr);

	/*
	 * This is the controlling variable for dpr_cv, so we must change it under
	 * the lock, even though we're not really *done* yet.  (The broadcast which
	 * sets everyone else going is at function's end.)
	 */
	dpr->dpr_done = B_TRUE;

	/*
	 * Only release this process if it was invasively traced. A
	 * noninvasively traced process's control thread will suicide while the
	 * process is still alive.
	 *
	 * This may do an unlock to unwind any active Ptrace() locking, so we
	 * may have to unlock, or may not.  We check the lock count afterwards
	 * to be sure, and force-unlock by resetting the count to 1 and then
	 * unlocking if need be.
	 *
	 * The unlock checks dpr_tid to figure out which lock count to adjust,
	 * so tell unlocking (no matter what route it's called by) that we're
	 * done and should null out the dpr_tid.
	 */
        dpr->dpr_ending = 1;
	if (dpr->dpr_proc && !suiciding) {
		Prelease(dpr->dpr_proc, dpr->dpr_created ? PS_RELEASE_KILL :
		    PS_RELEASE_NORMAL);
		proc_existed = 1;
	}

	if (dpr->dpr_lock_count_ctrl != 0) {
		dpr->dpr_lock_count_ctrl = 1;
		dt_proc_dpr_unlock(dpr);
	}
	dpr->dpr_ending = 0;
	dt_dprintf("%i: relinquished all locks.\n", dpr->dpr_pid);

	/*
	 * fd closing must be done with some care.  The thread may be cancelled
	 * before any of these fds have been assigned!
	 */

	if (dpr->dpr_fd)
	    close(dpr->dpr_fd);

	if (dpr->dpr_proxy_fd[0])
	    close(dpr->dpr_proxy_fd[0]);

	if (dpr->dpr_proxy_fd[1])
	    close(dpr->dpr_proxy_fd[1]);

	/*
	 * Death-notification queueing is complicated by the fact that we might
	 * have died due to failure to create or grab a process in the first
	 * place, which means both that the dpr will not be queued into the dpr
	 * hash and that dt_ps_proc_{grab,create}() will Pfree() it as soon as
	 * they notice that it's failed.  So we cannot enqueue the dpr in that
	 * case, and must enqueue a NULL instead.
	 *
	 * We also never want to emit a death notification for processes created
	 * via the internal API.
	 */
	if (!suiciding && dpr->dpr_notifiable)
		dt_proc_notify(dpr->dpr_hdl, dpr->dpr_hdl->dt_procs,
		    proc_existed ? dpr : NULL, NULL, B_TRUE, B_TRUE);

	/*
	 * A proxy request may have come in since the last time we checked for
	 * one: abort any such request with a notice that the process is not
	 * there any more.
	 */
	dpr->dpr_proxy_errno = ESRCH;
	dpr->dpr_proxy_rq = NULL;
	pthread_cond_signal(&dpr->dpr_msg_cv);

	/*
	 * Finally, signal on the cv, waking up any waiters.
	 */
	pthread_cond_broadcast(&dpr->dpr_cv);
}

/*
 * Reattach the ps_prochandle, destroying and recreating it.
 *
 * Must be called under the dpr_lock.
 */
static int
dt_proc_reattach(dtrace_hdl_t *dtp, dt_proc_t *dpr)
{
	int noninvasive = !Ptraceable(dpr->dpr_proc);
	dt_proc_notify_t *npr, **npp;
	dt_proc_hash_t *dph = dtp->dt_procs;
	int err;

	/*
	 * Take out the dph_lock around this entire operation, to ensure that
	 * notifications cannot be called with a stale dprn_dpr->dpr_proc.
	 */

	assert(MUTEX_HELD(&dpr->dpr_lock));
	assert(pthread_equal(dpr->dpr_lock_holder, pthread_self()));
	assert(pthread_equal(dpr->dpr_tid, pthread_self()));
	pthread_mutex_lock(&dph->dph_lock);

	Prelease(dpr->dpr_proc, PS_RELEASE_NO_DETACH);

	Pfree(dpr->dpr_proc);
	if ((dpr->dpr_proc = Pgrab(dpr->dpr_pid, noninvasive, 1,
		    dpr, &err)) == NULL) {
		dt_proc_error(dtp, dpr, "failed to regrab pid %li "
		    "after exec(): %s\n", (long) dpr->dpr_pid,
		    strerror(err));
		return(err);
	}
	Ptrace_set_detached(dpr->dpr_proc, dpr->dpr_created);
	Puntrace(dpr->dpr_proc, 0);

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

	return(0);
}

/*
 * Request that the dt_proc_loop() thread cease monitoring of the process, or
 * resume monitoring again.  Always called from the control thread, so we do not
 * need to worry about interrupting an existing monitoring round.
 */
static int dt_proc_monitor(dt_proc_t *dpr, int monitor)
{
	assert(pthread_equal(dpr->dpr_tid, pthread_self()));
	dpr->dpr_monitoring = monitor;
	return 0;
}

dt_proc_t *
dt_ps_proc_lookup(dtrace_hdl_t *dtp, struct ps_prochandle *P, int remove)
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

dt_proc_t *
dt_proc_lookup(dtrace_hdl_t *dtp, struct dtrace_prochandle *P, int remove)
{
	return dt_ps_proc_lookup(dtp, P->P, remove);
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
dt_ps_proc_retire(struct ps_prochandle *P)
{
	(void) Pclose(P);
}

/*
 * Determine if a process is retired.  Very cheap.
 */
static int
dt_ps_proc_retired(struct ps_prochandle *P)
{
	return (!Phasfds(P));
}

static void
dt_ps_proc_destroy(dtrace_hdl_t *dtp, struct ps_prochandle *P)
{
	dt_proc_t *dpr = dt_ps_proc_lookup(dtp, P, B_FALSE);
	dt_proc_hash_t *dph = dtp->dt_procs;
	dt_proc_notify_t *npr, **npp;

	/*
	 * Remove this dt_proc_t from the lookup hash, and then walk the
	 * dt_proc_hash_t's notification list and remove this dt_proc_t if it is
	 * enqueued.  If the dpr is already gone, do nothing.
	 *
	 * Note that we do not actually free the dpr: the caller must do that.
	 * (This is because the caller may need the dpr to exist while it
	 * navigates to the next item on the list to delete.)
	 */
	if (dpr == NULL)
		return;

	dt_dprintf("%s pid %d\n", dpr->dpr_created ? "killing" : "releasing",
		dpr->dpr_pid);

	pthread_mutex_lock(&dph->dph_lock);
	dt_ps_proc_lookup(dtp, P, B_TRUE);
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

	/*
	 * If the daemon thread is still alive, clean it up.
	 *
	 * Take out the lock around dpr_tid accesses, to ensure that we don't
	 * race with the setting of dpr_tid in dt_proc_control_cleanup().
	 *
	 * We must turn off background state change monitoring first,
	 * since cancellation triggers a libproc release, which flushes
	 * breakpoints and can wait on process state changes.
	 */
	dt_proc_dpr_lock(dpr);
	proxy_monitor(dpr, 0);
	if (dpr->dpr_tid) {
		/*
		 * Cancel the daemon thread, then wait for dpr_done to indicate
		 * the thread has exited.  (This will also terminate the
		 * process.)
		 */
		pthread_cancel(dpr->dpr_tid);

		while (!dpr->dpr_done)
			(void) pthread_cond_wait(&dpr->dpr_cv, &dpr->dpr_lock);

		dpr->dpr_lock_holder = pthread_self();
	} else if (dpr->dpr_proc) {
		/*
		 * The process control thread is already dead, but try to clean
		 * the process up anyway, just in case it survived to this
		 * point.  This can happen e.g. if the process was noninvasively
		 * grabbed and its control thread suicided.)
		 *
		 * It might be cleaned up already by dt_proc_exit_check().
		 */
		Prelease(dpr->dpr_proc, dpr->dpr_created ? PS_RELEASE_KILL :
		    PS_RELEASE_NORMAL);
	}
	dt_proc_dpr_unlock(dpr);

	if (!dt_ps_proc_retired(dpr->dpr_proc)) {
		assert(dph->dph_lrucnt != 0);
		dph->dph_lrucnt--;
	}
	dt_list_delete(&dph->dph_lrulist, dpr);
	dt_proc_remove(dtp, dpr->dpr_pid);
	Pfree(dpr->dpr_proc);

	pthread_cond_destroy(&dpr->dpr_cv);
	pthread_cond_destroy(&dpr->dpr_msg_cv);
	pthread_mutex_destroy(&dpr->dpr_lock);
}

static int
dt_proc_create_thread(dtrace_hdl_t *dtp, dt_proc_t *dpr, uint_t stop,
    int flags, const char *file, char *const *argv)
{
	dt_proc_control_data_t data;
	sigset_t nset, oset;
	pthread_attr_t a;
	int err;

	(void) pthread_mutex_lock(&dpr->dpr_lock);
	dpr->dpr_stop |= stop; /* set bit for initial rendezvous */
	dpr->dpr_monitoring = B_TRUE;
	if (flags & DTRACE_PROC_NOTIFIABLE)
		dpr->dpr_notifiable = 1;

	(void) pthread_attr_init(&a);
	(void) pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED);

	(void) sigfillset(&nset);
	(void) sigdelset(&nset, SIGABRT);	/* unblocked for assert() */

	data.dpcd_hdl = dtp;
	data.dpcd_proc = dpr;
	data.dpcd_start_proc = file;
	data.dpcd_start_proc_argv = argv;
	data.dpcd_flags = flags;

	if (pipe(data.dpcd_proxy_fd) < 0) {
		err = errno;
		(void) dt_proc_error(dpr->dpr_hdl, dpr,
		    "failed to create communicating pipe for pid %d: %s\n",
		    (int)dpr->dpr_pid, strerror(err));

		(void) pthread_mutex_unlock(&dpr->dpr_lock);
		(void) pthread_attr_destroy(&a);
		return (err);
	}

	(void) pthread_sigmask(SIG_SETMASK, &nset, &oset);
	err = pthread_create(&dpr->dpr_tid, &a, dt_proc_control, &data);
	(void) pthread_sigmask(SIG_SETMASK, &oset, NULL);

	/*
	 * If the control thread was created, then wait on dpr_cv for either
	 * dpr_done to be set (the victim died, the control thread failed, or no
	 * control thread was ultimately needed) or DT_PROC_STOP_IDLE to be set,
	 * indicating that the victim is now stopped and the control thread is
	 * at the rendezvous event.  On success, we return with the process and
	 * control thread stopped: the caller can then apply
	 * dt_ps_proc_continue() to resume both.
	 */
	if (err == 0) {
		while (!dpr->dpr_done && !(dpr->dpr_stop & DT_PROC_STOP_IDLE))
			(void) pthread_cond_wait(&dpr->dpr_cv, &dpr->dpr_lock);

		dpr->dpr_lock_holder = pthread_self();

		/*
		 * If dpr_done is set, the control thread aborted before it
		 * reached the rendezvous event; if this happened because the
		 * monitored process is dead, note as much.
		 */
		if (dpr->dpr_done)
			if (!dpr->dpr_proc) {
				err = ESRCH; /* cause grab() or create() to fail */
				dt_set_errno(dtp, err);
			}
	} else {
		dt_proc_error(dpr->dpr_hdl, dpr,
		    "failed to create control thread for pid %d: %s\n",
		    (int)dpr->dpr_pid, strerror(err));
	}

	(void) pthread_mutex_unlock(&dpr->dpr_lock);
	(void) pthread_attr_destroy(&a);

	return (err);
}

struct ps_prochandle *
dt_ps_proc_create(dtrace_hdl_t *dtp, const char *file, char *const *argv,
	int flags)
{
	dt_proc_hash_t *dph = dtp->dt_procs;
	dt_proc_t *dpr;
	pthread_mutexattr_t attr;
	pthread_mutexattr_t *attrp = NULL;

	if ((dpr = dt_zalloc(dtp, sizeof (dt_proc_t))) == NULL)
		return (NULL); /* errno is set for us */

	if (_dtrace_debug_assert & DT_DEBUG_MUTEXES) {
		attrp = &attr;
		pthread_mutexattr_init(attrp);
		pthread_mutexattr_settype(attrp, PTHREAD_MUTEX_ERRORCHECK);
	}

	(void) pthread_mutex_init(&dpr->dpr_lock, attrp);
	(void) pthread_cond_init(&dpr->dpr_cv, NULL);
	(void) pthread_cond_init(&dpr->dpr_msg_cv, NULL);

	if (_dtrace_debug_assert & DT_DEBUG_MUTEXES) {
		pthread_mutexattr_destroy(attrp);
		attrp = NULL;
	}

	/*
	 * Newly-created processes should be invasively grabbed.
	 */
	if (flags & DTRACE_PROC_SHORTLIVED)
		flags &= ~DTRACE_PROC_SHORTLIVED;

	dpr->dpr_hdl = dtp;
	dpr->dpr_created = B_TRUE;

	if (dt_proc_create_thread(dtp, dpr, dtp->dt_prcmode, flags,
		file, argv) != 0) {

		pthread_cond_destroy(&dpr->dpr_cv);
		pthread_cond_destroy(&dpr->dpr_msg_cv);
		pthread_mutex_destroy(&dpr->dpr_lock);
		dt_free(dtp, dpr);

		return (NULL); /* dt_proc_error() has been called for us */
	}

	dph->dph_lrucnt++;
	dpr->dpr_hash = dph->dph_hash[dpr->dpr_pid & (dph->dph_hashlen - 1)];
	dph->dph_hash[dpr->dpr_pid & (dph->dph_hashlen - 1)] = dpr;
	dt_list_prepend(&dph->dph_lrulist, dpr);

	dt_dprintf("created pid %d\n", (int)dpr->dpr_pid);
	dpr->dpr_refs++;

	/*
	 * If requested, wait for the control thread to finish initialization
	 * and rendezvous.
	 */
	if (flags & DTRACE_PROC_WAITING)
		dt_ps_proc_continue(dtp, dpr->dpr_proc);

	return (dpr->dpr_proc);
}

struct dtrace_prochandle
dt_proc_create(dtrace_hdl_t *dtp, const char *file, char *const *argv,
	int flags)
{
	struct dtrace_prochandle P;
	P.P = dt_ps_proc_create(dtp, file, argv, flags);
	return P;
}

struct ps_prochandle *
dt_ps_proc_grab(dtrace_hdl_t *dtp, pid_t pid, int flags)
{
	dt_proc_hash_t *dph = dtp->dt_procs;
	uint_t h = pid & (dph->dph_hashlen - 1);
	dt_proc_t *dpr, *opr;
	pthread_mutexattr_t attr;
	pthread_mutexattr_t *attrp = NULL;

	/*
	 * Search the hash table for the pid.  If it is already grabbed or
	 * created, move the handle to the front of the lrulist, increment
	 * the reference count, and return the existing ps_prochandle.
	 *
	 * If it is retired, bring it out of retirement aggressively, so as to
	 * ensure that dph_lrucnt and dt_ps_proc_retired() do not get out of synch
	 * (which would cause aggressive early retirement of processes even when
	 * unnecessary).
	 *
	 * If it is noninvasive, or the process is dead, and the request was for
	 * an invasive grab, destroy it and make a new one (if the process is
	 * dead, this will obviously fail).  This destruction is safe because
	 * we know there is no control thread, so it is impossible for anything
	 * to be holding a reference to it.
	 */
	for (dpr = dph->dph_hash[h]; dpr != NULL; dpr = dpr->dpr_hash) {
		if ((dpr->dpr_pid == pid) &&
		    !(flags & DTRACE_PROC_SHORTLIVED) && !dpr->dpr_tid) {
				dt_dprintf("pid %d (cached, but noninvasive) "
				    "dropped.\n", (int)pid);

				dt_list_delete(&dph->dph_lrulist, dpr);
				dt_ps_proc_destroy(dtp, dpr->dpr_proc);
				dt_free(dtp, dpr);

		} else if (dpr->dpr_pid == pid) {
			dt_dprintf("grabbed pid %d (cached)\n", (int)pid);

			dt_list_delete(&dph->dph_lrulist, dpr);
			dt_list_prepend(&dph->dph_lrulist, dpr);
			dpr->dpr_refs++;

			if (dt_ps_proc_retired(dpr->dpr_proc)) {
				/* not retired any more */
				(void) Pmemfd(dpr->dpr_proc);
				dph->dph_lrucnt++;
			}
			return (dpr->dpr_proc);
		}
	}

	/*
	 * Quick check if the process is dead, catering for short-lived
	 * processes and ustack().  This avoids forking off a lot of short-lived
	 * threads to check the same process time and again.
	 */
	if (!Pexists(pid)) {
		dt_dprintf("Pgrab(%d): Process does not exist, cannot grab\n",
		    pid);
		errno = ESRCH;
		dt_set_errno(dtp, errno);
		return NULL;
	}

	if ((dpr = dt_zalloc(dtp, sizeof (dt_proc_t))) == NULL)
		return (NULL); /* errno is set for us */

	if (_dtrace_debug_assert & DT_DEBUG_MUTEXES) {
		attrp = &attr;
		pthread_mutexattr_init(attrp);
		pthread_mutexattr_settype(attrp, PTHREAD_MUTEX_ERRORCHECK);
	}

	(void) pthread_mutex_init(&dpr->dpr_lock, attrp);
	(void) pthread_cond_init(&dpr->dpr_cv, NULL);
	(void) pthread_cond_init(&dpr->dpr_msg_cv, NULL);

	if (_dtrace_debug_assert & DT_DEBUG_MUTEXES) {
		pthread_mutexattr_destroy(attrp);
		attrp = NULL;
	}

	dpr->dpr_hdl = dtp;
	dpr->dpr_pid = pid;
	dpr->dpr_created = B_FALSE;

	/*
	 * Create a control thread for this process and store its ID in
	 * dpr->dpr_tid.
	 */
	if (dt_proc_create_thread(dtp, dpr, DT_PROC_STOP_GRAB, flags,
		NULL, NULL) != 0) {

		pthread_cond_destroy(&dpr->dpr_cv);
		pthread_cond_destroy(&dpr->dpr_msg_cv);
		pthread_mutex_destroy(&dpr->dpr_lock);
		dt_free(dtp, dpr);

		return (NULL); /* dt_proc_error() has been called for us */
	}

	dph->dph_lrucnt++;
	dpr->dpr_hash = dph->dph_hash[h];
	dph->dph_hash[h] = dpr;
	dt_list_prepend(&dph->dph_lrulist, dpr);

	dt_dprintf("grabbed pid %d\n", (int)pid);
	dpr->dpr_refs++;

	/*
	 * If we're currently caching more processes than dph_lrulim permits,
	 * attempt to find the least-recently-used process that is currently
	 * unreferenced and has not already been retired, and retire it.  (This
	 * does not actually delete it, because its presence is still necessary
	 * to ensure that we do put it into halted state again.  It merely
	 * closes any associated filehandles.)
	 *
	 * We know this expiry run cannot affect the handle currently being
	 * grabbed, since we have already boosted its refcnt.
	 */
	if (dph->dph_lrucnt > dph->dph_lrulim) {
		for (opr = dt_list_prev(&dph->dph_lrulist);
		     opr != NULL; opr = dt_list_prev(opr)) {
			if (opr->dpr_refs == 0 && !dt_ps_proc_retired(opr->dpr_proc)) {
				dt_ps_proc_retire(opr->dpr_proc);
				dph->dph_lrucnt--;
				break;
			}
		}
	}

	/*
	 * If requested, wait for the control thread to finish initialization
	 * and rendezvous.  (This will have no effect on a noninvasively-grabbed
	 * process, which is already running in any case.)
	 */
	if (flags & DTRACE_PROC_WAITING)
		dt_ps_proc_continue(dtp, dpr->dpr_proc);

	return (dpr->dpr_proc);
}

struct dtrace_prochandle
dt_proc_grab(dtrace_hdl_t *dtp, pid_t pid, int flags)
{
	struct dtrace_prochandle P;
	P.P = dt_ps_proc_grab(dtp, pid, flags);
	return P;
}

void
dt_proc_release(dtrace_hdl_t *dtp, struct dtrace_prochandle *P)
{
	dt_proc_t *dpr = dt_proc_lookup(dtp, P, B_FALSE);
	dt_proc_hash_t *dph = dtp->dt_procs;

	assert(dpr != NULL);
	assert(dpr->dpr_refs != 0);

	if (--dpr->dpr_refs == 0 &&
	    (dph->dph_lrucnt > dph->dph_lrulim) &&
	    !dt_ps_proc_retired(dpr->dpr_proc)) {
		dt_ps_proc_retire(P->P);
		dph->dph_lrucnt--;
	}

	if (dpr->dpr_done) {
		dt_ps_proc_destroy(dtp, P->P);
		dt_free(dtp, dpr);
	}
}

static long
dt_ps_proc_continue(dtrace_hdl_t *dtp, struct ps_prochandle *P)
{
	dt_proc_t *dpr = dt_ps_proc_lookup(dtp, P, B_FALSE);

	/*
	 * Noninvasively-grabbed processes are never stopped by us, so
	 * continuing them is meaningless.  The same is true of processes with
	 * dead control threads for whatever reason.
	 */
	if ((dpr->dpr_done) || (!dpr->dpr_proc) || (!dpr->dpr_tid) ||
	    (!Ptraceable(dpr->dpr_proc)))
		return 0;

	(void) pthread_mutex_lock(&dpr->dpr_lock);

	dt_dprintf("%i: doing a dt_ps_proc_continue().\n", dpr->dpr_pid);

	/*
	 * Calling dt_ps_proc_continue() from the control thread is banned.
	 */
	assert(!pthread_equal(dpr->dpr_tid, pthread_self()));

	/*
	 * A continue has two phases.  First, we send a signal down the proxy
	 * pipe to tell the control thread to awaken its child; then we wait for
	 * its cv signal to tell us that it has completed detaching that child.
	 * Without this, we may grab the dpr_lock before it can be re-grabbed by
	 * the control thread and used to detach, leading to unbalanced
	 * Ptrace()/Puntrace() calls, a child permanently stuck in PS_TRACESTOP,
	 * and a rapid deadlock.
	 *
	 * This can only be called once for a given process: once the process
	 * has been resumed, that's it.
	 */

	if (dpr->dpr_stop & DT_PROC_STOP_RESUMED) {
		dt_dprintf("%i: Already resumed, returning.\n",
			dpr->dpr_pid);
		return 0;
	}

	if (dpr->dpr_stop & DT_PROC_STOP_IDLE) {
		char junk = '\0'; /* unimportant */

		dpr->dpr_stop &= ~DT_PROC_STOP_IDLE;
		dpr->dpr_proxy_rq = dt_proc_continue;
		errno = 0;
		while (write(dpr->dpr_proxy_fd[1], &junk, 1) < 0 && errno == EINTR);
		if (errno != 0 && errno != EINTR) {
			dt_proc_error(dpr->dpr_hdl, dpr, "Cannot write to "
			    "proxy pipe for dt_ps_proc_continue(), deadlock is "
			    "certain: %s\n", strerror(errno));
			return (-1);
		}
	}

	while (!dpr->dpr_done && !(dpr->dpr_stop & DT_PROC_STOP_RESUMED))
		pthread_cond_wait(&dpr->dpr_cv, &dpr->dpr_lock);

	dt_dprintf("%i: dt_ps_proc_continue()d.\n", dpr->dpr_pid);

	(void) pthread_mutex_unlock(&dpr->dpr_lock);

	return 0;
}

long
dt_proc_continue(dtrace_hdl_t *dtp, struct dtrace_prochandle *P)
{
	/*
	 * Note: we intentionally do not call proxy_monitor() to suppress
	 * background Pwait()ing here, since we are relying on exactly that
	 * background Pwait()ing to tell when the process has reached a
	 * continuable state.
	 */

	return dt_ps_proc_continue(dtp, P->P);
}

void
dt_proc_lock(dtrace_hdl_t *dtp, struct dtrace_prochandle *P)
{
	dt_proc_dpr_lock(dt_proc_lookup(dtp, P, B_FALSE));
}

void
dt_proc_unlock(dtrace_hdl_t *dtp, struct dtrace_prochandle *P)
{
	dt_proc_dpr_unlock(dt_proc_lookup(dtp, P, B_FALSE));
}

static void
dt_proc_dpr_lock(dt_proc_t *dpr)
{
	unsigned long *lock_count;

	if (pthread_equal(pthread_self(), dpr->dpr_tid))
		lock_count = &dpr->dpr_lock_count_ctrl;
	else
		lock_count = &dpr->dpr_lock_count_main;

	if (!pthread_equal(dpr->dpr_lock_holder, pthread_self()) ||
	    *lock_count == 0) {
		dt_dprintf("%i: Taking out lock\n", dpr->dpr_pid);
		pthread_mutex_lock(&dpr->dpr_lock);
		dpr->dpr_lock_holder = pthread_self();
		dt_dprintf("%i: Taken out lock\n", dpr->dpr_pid);
	}

	assert(MUTEX_HELD(&dpr->dpr_lock));
	assert(*lock_count == 0 ||
	    pthread_equal(dpr->dpr_lock_holder, pthread_self()));

	(*lock_count)++;
}

static void
dt_proc_dpr_unlock(dt_proc_t *dpr)
{
	int err;
	unsigned long *lock_count;

	if (pthread_equal(pthread_self(), dpr->dpr_tid))
		lock_count = &dpr->dpr_lock_count_ctrl;
	else
		lock_count = &dpr->dpr_lock_count_main;

	assert(pthread_equal(dpr->dpr_lock_holder, pthread_self()) &&
	    *lock_count > 0);
	(*lock_count)--;

	/*
	 * A subtlety.	When a control thread dies at the same instant as dtrace
	 * shutdown, we set dpr_tid to zero to indicate its death.  This must be
	 * done under the dpr_lock, to stop dt_ps_proc_destroy() from racing
	 * with the dpr_tid change -- but we check dpr_tid just above!	So we
	 * must reset it here, after we've checked it, but before we actually
	 * unlock the lock.
	 */
	if (*lock_count == 0) {
		dt_dprintf("%i: Relinquishing lock\n", dpr->dpr_pid);

		if (dpr->dpr_ending)
			dpr->dpr_tid = 0;

		err = pthread_mutex_unlock(&dpr->dpr_lock);
		assert(err == 0); /* check for unheld lock */
	} else
		assert(MUTEX_HELD(&dpr->dpr_lock));
}

static void
dt_proc_ptrace_lock(struct ps_prochandle *P, void *arg, int ptracing)
{
	dt_proc_t *dpr = arg;

	if (ptracing)
		dt_proc_dpr_lock(dpr);
	else
		dt_proc_dpr_unlock(dpr);
}

/*
 * Define the public interface to a libproc function from the rest of DTrace,
 * automatically proxying via the process-control thread and retrying on
 * exec().
 */
#define DEFINE_dt_Pfunction(function, err_ret, ...)		   \
	dt_proc_t * volatile dpr = dt_proc_lookup(dtp, P, B_FALSE); \
	jmp_buf this_exec_jmp, *old_exec_jmp; \
	\
	assert(MUTEX_HELD(&dpr->dpr_lock)); \
	old_exec_jmp = unwinder_pad; \
	if (setjmp(this_exec_jmp)) { \
		unwinder_pad = &this_exec_jmp; \
		if (!proxy_reattach(dpr)) \
			return (err_ret); \
		P->P = dpr->dpr_proc; \
	} \
	unwinder_pad = &this_exec_jmp; \
	proxy_monitor(dpr, 0); \
	ret = function(P->P, __VA_ARGS__); \
	proxy_monitor(dpr, 1); \
	unwinder_pad = old_exec_jmp;

int
dt_Plookup_by_addr(dtrace_hdl_t *dtp, struct dtrace_prochandle *P,
    uintptr_t addr, char *sym_name_buffer, size_t bufsize,
    GElf_Sym *symbolp)
{
	int ret;
	DEFINE_dt_Pfunction(Plookup_by_addr, -1, addr, sym_name_buffer, bufsize, symbolp);
	return ret;
}

const prmap_t *
dt_Paddr_to_map(dtrace_hdl_t *dtp, struct dtrace_prochandle *P, uintptr_t addr)
{
	const prmap_t *ret;
	DEFINE_dt_Pfunction(Paddr_to_map, NULL, addr);
	return ret;
}

const prmap_t *
dt_Pname_to_map(dtrace_hdl_t *dtp, struct dtrace_prochandle *P,
    const char *name)
{
	const prmap_t *ret;
	DEFINE_dt_Pfunction(Pname_to_map, NULL, name);
	return ret;
}

const prmap_t *
dt_Plmid_to_map(dtrace_hdl_t *dtp, struct dtrace_prochandle *P, Lmid_t lmid,
    const char *name)
{
	const prmap_t *ret;
	DEFINE_dt_Pfunction(Plmid_to_map, NULL, lmid, name);
	return ret;
}

char *
dt_Pobjname(dtrace_hdl_t *dtp, struct dtrace_prochandle *P, uintptr_t addr,
	char *buffer, size_t bufsize)
{
	char *ret;
	DEFINE_dt_Pfunction(Pobjname, NULL, addr, buffer, bufsize);
	return ret;
}

int
dt_Plmid(dtrace_hdl_t *dtp, struct dtrace_prochandle *P, uintptr_t addr,
    Lmid_t *lmidp)
{
	int ret;
	DEFINE_dt_Pfunction(Plmid, -1, addr, lmidp);
	return ret;
}

int
dt_Pxlookup_by_name(dtrace_hdl_t *dtp, struct dtrace_prochandle *P, Lmid_t lmid,
    const char *oname, const char *sname, GElf_Sym *symp, prsyminfo_t *sip)
{
	int ret;
	DEFINE_dt_Pfunction(Pxlookup_by_name, -1, lmid, oname, sname, symp,
	    sip);
	return ret;
}

int
dt_Pwritable_mapping(dtrace_hdl_t *dtp, struct dtrace_prochandle *P,
    uintptr_t addr)
{
	int ret;
	DEFINE_dt_Pfunction(Pwritable_mapping, -1, addr);
	return ret;
}

int
dt_Psymbol_iter_by_addr(dtrace_hdl_t *dtp, struct dtrace_prochandle *P,
    const char *object_name, int which, int mask, proc_sym_f *func, void *cd)
{
	int ret;
	DEFINE_dt_Pfunction(Psymbol_iter_by_addr, -1, object_name, which,
	    mask, func, cd);
	return ret;
}

int
dt_Pobject_iter(dtrace_hdl_t *dtp, struct dtrace_prochandle *P,
    proc_map_f *func, void *cd)
{
	int ret;
	DEFINE_dt_Pfunction(Pobject_iter, -1, func, cd);
	return ret;
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
	dt_proc_t *dpr, *old_dpr = NULL;
	dt_proc_notify_t *npr, **npp;

	for (dpr = dt_list_next(&dph->dph_lrulist);
	     dpr != NULL; dpr = dt_list_next(dpr)) {
		dt_ps_proc_destroy(dtp, dpr->dpr_proc);
		dt_free(dtp, old_dpr);
		old_dpr = dpr;
	}
	dt_free(dtp, old_dpr);

	/*
	 * Some notification enqueues may have been missed by
	 * dt_ps_proc_destroy(), notably those with a NULL dpr that result from
	 * notifications that process attachment failed.
	 */
	npp = &dph->dph_notify;
	while ((npr = *npp) != NULL) {
		*npp = npr->dprn_next;
		dt_free(dtp, npr);
	}
	pthread_cond_destroy(&dph->dph_cv);

	dtp->dt_procs = NULL;
	dt_free(dtp, dph);
}

struct dtrace_prochandle
dtrace_proc_create(dtrace_hdl_t *dtp, const char *file, char *const *argv,
	int flags)
{
	struct dtrace_prochandle P;

	dt_ident_t *idp = dt_idhash_lookup(dtp->dt_macros, "target");
	struct ps_prochandle *ps_P = dt_ps_proc_create(dtp, file, argv,
	    flags | DTRACE_PROC_NOTIFIABLE);

	if (ps_P != NULL && idp != NULL && idp->di_id == 0)
		idp->di_id = Pgetpid(ps_P); /* $target = created pid */

	P.P = ps_P;
	return P;
}

struct dtrace_prochandle
dtrace_proc_grab(dtrace_hdl_t *dtp, pid_t pid, int flags)
{
	struct dtrace_prochandle P;

	dt_ident_t *idp = dt_idhash_lookup(dtp->dt_macros, "target");
	struct ps_prochandle *ps_P = dt_ps_proc_grab(dtp, pid,
	    flags | DTRACE_PROC_NOTIFIABLE);

	if (ps_P != NULL && idp != NULL && idp->di_id == 0)
		idp->di_id = pid; /* $target = grabbed pid */

	P.P = ps_P;
	return P;
}

void
dtrace_proc_release(dtrace_hdl_t *dtp, struct dtrace_prochandle *P)
{
	dt_proc_release(dtp, P);
}

void
dtrace_proc_continue(dtrace_hdl_t *dtp, struct dtrace_prochandle *P)
{
	dt_proc_continue(dtp, P);
}
