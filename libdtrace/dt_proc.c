/*
 * Oracle Linux DTrace.
 * Copyright (c) 2010, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
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
 * dtrace_proc_grab_pid/dtrace_proc_create mechanisms.  Like all exported
 * libdtrace calls, these are assumed to be MT-Unsafe.  MT-Safety is ONLY
 * provided for calls via the libproc marshalling layer.  All calls from the
 * rest of DTrace must go via the dt_P*() functions, which in addition to
 * routing calls via the proxying layer also arrange to automatically retry in
 * the event of child exec().
 *
 * The ps_prochandles themselves are maintained along with a dt_proc_t struct in
 * a hash table indexed by PID.  This provides basic locking and reference
 * counting.  The dt_proc_t is also maintained in LRU order on dph_lrulist.  The
 * dph_lrucnt and dph_lrulim count the number of processes we have grabbed or
 * created but not retired, and the current limit on the number of actively
 * cached entries.
 *
 * The control threads currently invoke processes, resume them when
 * dt_proc_continue() is called, manage ptrace()-related signal dispatch and
 * breakpoint handling tasks, handle libproc requests from the rest of DTrace
 * relating to their specific process, and notify interested parties when the
 * process dies.
 *
 * A simple notification mechanism is provided for libdtrace clients using
 * dtrace_handle_proc() for notification of process death.  When this event
 * occurs, the dt_proc_t itself is enqueued on a notification list and the
 * control thread triggers an event using dtp->dt_prov_fd.  The epoll_wait()
 * in dtrace_consume() will wake up using this condition and the client handler
 * will be called as necessary.
 *
 * The locking in this file is crucial, to stop the process-control threads
 * from running before dtrace is ready for them, to coordinate proxy calls
 * between the main thread and process-control thread, and to ensure that the
 * state is not torn down while the process-control threads are still using it.
 * Two locks are used:
 *  - the dph_lock is a simple mutex protecting mutations of the dph notify
 *    list; the dph hash itself is not protected, and may only be modified
 *    from the main thread.  This lock nests inside the dpr_lock if both are
 *    taken at once.
 *  - the dpr_lock is a counted semaphore constructed from a mutex, a
 *    currently-holding thread ID, and two counters tracking a lock count for
 *    each of its two possible holders (it could equally well be constructed
 *    with one counter that counts up for one holder and down for the other).
 *    It is taken around all dpr operations and dropped around proxy calls,
 *    ensuring that the process-control thread and main thread do not race
 *    with each other. It is also used as the lock around the dpr_cv (the
 *    condvar for explicit waiting operations involving dt_proc_stop()/
 *    dt_proc_resume()), and around the dpr_msg_cv (the condvar for proxy
 *    operations).  Using one mutex for two cvs might seem troublesome, but we
 *    are saved by the fact that the main thread can only ever be doing one of
 *    these at once, and that the proxy cv is used in very stereotyped ways
 *    (proxy_call()->dt_proc_loop(), wwith a special case for cleanup).
 */

#include <sys/wait.h>
#include <sys/eventfd.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include <errno.h>
#include <port.h>
#include <poll.h>
#include <setjmp.h>

#include <libproc.h>
#include <dt_proc.h>
#include <dt_pid.h>
#include <dt_impl.h>

enum dt_attach_time_t { ATTACH_START, ATTACH_ENTRY, ATTACH_FIRST_ARG_MAIN,
			ATTACH_DIRECT_MAIN };

static dt_proc_t *dt_proc_create(dtrace_hdl_t *, const char *, char *const *,
    int flags);
static dt_proc_t *dt_proc_grab(dtrace_hdl_t *dtp, pid_t pid, int flags);
static dt_proc_t *dt_proc_lookup_remove(dtrace_hdl_t *dtp, pid_t pid,
    int remove);
static int dt_proc_attach_break(dt_proc_t *dpr, enum dt_attach_time_t attach_time);
static int dt_proc_reattach(dtrace_hdl_t *dtp, dt_proc_t *dpr);
static int dt_proc_monitor(dt_proc_t *dpr, int monitor);
static void dt_proc_scan(dtrace_hdl_t *dtp, dt_proc_t *dpr);
static int dt_proc_loop(dt_proc_t *dpr, int awaiting_continue);
static void dt_main_fail_rendezvous(dt_proc_t *dpr);
static void dt_proc_ptrace_lock(struct ps_prochandle *P, void *arg,
    int ptracing);
static void dt_proc_waitpid_lock(struct ps_prochandle *P, void *arg,
    int waitpidding);
static long dt_proc_continue(dtrace_hdl_t *dtp, dt_proc_t *dpr);

/*
 * Locking assertions.
 */
#define assert_self_locked(dpr)			\
	do { \
		assert(MUTEX_HELD(&dpr->dpr_lock));		\
		assert(pthread_equal(dpr->dpr_lock_holder, pthread_self())); \
	} while (0)

/*
 * The default internal signal value.
 */
static int internal_proc_signal = -1;

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
	       pid_t pid, const char *msg)
{
	dt_proc_notify_t *dprn = dt_alloc(dtp, sizeof(dt_proc_notify_t));

	if (dprn == NULL) {
		dt_dprintf("failed to allocate notification for %d %s\n",
		    (int)dpr->dpr_pid, msg ? msg : "");
	} else {
		dprn->dprn_dpr = dpr;
		if (msg == NULL)
			dprn->dprn_errmsg[0] = '\0';
		else
			strlcpy(dprn->dprn_errmsg, msg,
			    sizeof(dprn->dprn_errmsg));

		pthread_mutex_lock(&dph->dph_lock);

		dprn->dprn_next = dph->dph_notify;
		dprn->dprn_pid = pid;
		dph->dph_notify = dprn;

		eventfd_write(dtp->dt_proc_fd, 1);
		pthread_mutex_unlock(&dph->dph_lock);
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
		    "for resume.\n", (int)dpr->dpr_pid);
	}
}

/*
 * After a stop is carried out and we have carried out any operations that must
 * be done serially, we must signal back to the process waiting in
 * dt_proc_continue() that it can resume.
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
		dt_proc_notify(dtp, dtp->dt_procs, dpr, dpr->dpr_pid,
			       dpr->dpr_errmsg);
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
	rd_agent_t *agent = Prd_agent(dpr->dpr_proc);
	if (agent != NULL)
		rd_event_enable(agent, dt_proc_rdevent, dpr);
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
dt_proc_attach_break(dt_proc_t *dpr, enum dt_attach_time_t attach_time)
{
	uintptr_t addr = 0;
	GElf_Sym sym;
	dtrace_hdl_t *dtp = dpr->dpr_hdl;
	int (*handler)(uintptr_t addr, void *data) = dt_break_interesting;

	assert(MUTEX_HELD(&dpr->dpr_lock));

	dt_proc_rdagent(dpr);

	dt_dprintf("Called dt_attach() with attach_time %i\n", attach_time);

	/*
	 * If we're stopping on exec we have no breakpoints to drop: if
	 * we're stopping on preinit and it's after the dt_proc_continue()
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
		 * Before dt_proc_continue().  Preinit, postinit and main all
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
		 * After dt_proc_continue(), stopped at __libc_start_main().
		 * main()s address is passed as the first argument to this
		 * function.
		 */
		dt_dprintf("pid %d: dropping breakpoint on address of "
		    "__libc_start_main's first arg\n", (int)dpr->dpr_pid);

		addr = Pread_first_arg(dpr->dpr_proc);
		if (addr == (uintptr_t)-1) {
			dt_dprintf("Cannot look up __libc_start_main()'s "
			    "first arg: %s\n", strerror(errno));
			return -1;
		}
		break;
	case ATTACH_DIRECT_MAIN:
		/*
		 * After dt_proc_continue().  Drop a breakpoint on main(),
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

	dt_set_errno(dtp, EDT_COMPILER);
	return NULL;
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
 *
 * The actual call involves
 *  - write a message down the proxy pipe.
 *  - hit the control thread with the dt_proc_signal, which will wake up
 *    the waitpid() in Pwait() if it's waiting there and force an early exit to
 *    check the proxy pipe, and will set the thread-local waitpid_interrupted
 *    variable wich Pwait() checks before it enters waitpid().
 *  - arm a timer which repeatedly hits the control thread with this signal;
 *    it is disarmed by the control thread.
 *  - wait for the dpr_proxy_rq to be reset and the dpr_msg_cv to be
 *    signalled, indciating that the request is done.
 *  - if an exec() has happened, jump out to the dpr_proxy_exec_retry
 *    pad, which will attempt to reattach to the new process.
 */

static long
proxy_call(dt_proc_t *dpr, long (*proxy_rq)(), int exec_retry)
{
	char junk = '\0'; /* unimportant */
	struct itimerspec pinger = {0};
	struct itimerspec nonpinger = {0};

	dpr->dpr_proxy_rq = proxy_rq;

	/*
	 * We may have blocked on lock acquisition while a process termination
	 * is under way.  Note this.
	 */
	if (dpr->dpr_done) {
		errno = ESRCH;
		return -1;
	}

	errno = 0;
	while (write(dpr->dpr_proxy_fd[1], &junk, 1) < 0 && errno == EINTR);
	if (errno != 0 && errno != EINTR) {
		dt_proc_error(dpr->dpr_hdl, dpr, "Cannot write to proxy pipe "
		    "for Pwait(), deadlock is certain: %s\n", strerror(errno));
		return -1;
	}
	pthread_kill(dpr->dpr_tid, dpr->dpr_hdl->dt_proc_signal);

	/*
	 * This timer's sole purpose is to hit the control thread with a signal
	 * if we are unlucky enough for the initial signal to strike in the gap
	 * between checking if the signal has hit and entering the waitpid().
	 * If this race hits, the proxy call latency will be at least as great
	 * as the interval of the timer, so it shouldn't be too long: but the
	 * value isn't that important otherwise.  (If it's too short, it'll
	 * waste time delivering useless signals.)
	 *
	 * Because this is only solving a rare race, if it can't be armed it's
	 * not too serious a problem, and we can more or less just keep going.
	 */
	pinger.it_value.tv_nsec = 1000000; /* arbitrary, not too long: 1ms */
	pinger.it_interval.tv_nsec = 1000000;
	if (timer_settime(dpr->dpr_proxy_timer, 0, &pinger, NULL) < 0)
		dt_proc_error(dpr->dpr_hdl, dpr,
			      "Cannot create fallback wakeup timer: %s\n",
			      strerror(errno));

	while (dpr->dpr_proxy_rq != NULL)
		pthread_cond_wait(&dpr->dpr_msg_cv, &dpr->dpr_lock);

	/*
	 * Disarm the timer again.  This is also done from
	 * dt_proc_waitpid_lock() so that the signal stops as soon as the
	 * waitpid() is done: but if the control thread was not waiting at
	 * waitpid() at all, we'll want to disarm it regardless.
	 *
	 * From this point on, a substantial delay may have happened, so we need
	 * to consider that the process may have terminated, in which case dpr
	 * will still be allocated but most other things will be freed (like the
	 * timer).
	 */
	if (!dpr->dpr_done &&
	    timer_settime(dpr->dpr_proxy_timer, 0, &nonpinger, NULL) < 0)
		dt_proc_error(dpr->dpr_hdl, dpr,
			      "Cannot disarm fallback wakeup timer: %s\n",
			      strerror(errno));

	dpr->dpr_lock_holder = pthread_self();

	if (!dpr->dpr_done && exec_retry && dpr->dpr_proxy_exec_retry &&
	    *unwinder_pad)
		longjmp(*unwinder_pad, dpr->dpr_proxy_exec_retry);

	errno = dpr->dpr_proxy_errno;
	return dpr->dpr_proxy_ret;
}

static long
proxy_pwait(struct ps_prochandle *P, void *arg, boolean_t block,
    int *return_early)
{
	dt_proc_t *dpr = arg;

	assert_self_locked(dpr);

	/*
	 * If we are already in the right thread, pass the call straight on.
	 *
	 * Otherwise, proxy it, throwing out the return_early arg because
	 * it is only used for internal communication between the monitor
	 * thread and Pwait() itself.
	 */
	if (pthread_equal(dpr->dpr_tid, pthread_self()))
		return Pwait_internal(P, block, return_early);

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

/*
 * This proxy request requests that the controlling thread resume the process
 * and terminate.  It's also used by other proxy requests' error-handling loops,
 * where a proxy response and immediate thread termination is called for.
 *
 * This request cannot trigger an exec-retry, as it does not monitor process
 * state changes.
 */
static long
proxy_quit(dt_proc_t *dpr, int err)
{
	assert_self_locked(dpr);

	/*
	 * If we are already in the right thread, respond to the proxy message
	 * and terminate the thread.  We do not unlock at all: the unlock
	 * happens late in the cleanup handler, by which point the thread has
	 * finished tidying up after itself.
	 */
	if (pthread_equal(dpr->dpr_tid, pthread_self())) {
		dpr->dpr_proxy_errno = err;
		dpr->dpr_proxy_rq = NULL;
		dpr->dpr_proxy_ret = err;
		pthread_cond_signal(&dpr->dpr_msg_cv);
		pthread_exit(NULL);
	}

	dpr->dpr_proxy_args.dpr_quit.err = err;
	return proxy_call(dpr, proxy_quit, 0);
}

static __thread int waitpid_interrupted;

static void
waitpid_interrupting_handler(int sig)
{
	waitpid_interrupted = 1;
}

/*
 * Set up and tear down the signal handler (above) used to force waitpid() to
 * abort with -EINTR.
 */
void
dt_proc_signal_init(dtrace_hdl_t *dtp)
{
	struct sigaction act;

	if (internal_proc_signal == -1)
		internal_proc_signal = 0;

	memset(&act, 0, sizeof(act));
	act.sa_handler = waitpid_interrupting_handler;
	dtp->dt_proc_signal = SIGRTMIN + internal_proc_signal;
	sigaction(dtp->dt_proc_signal, &act, &dtp->dt_proc_oact);
}

void
dt_proc_signal_fini(dtrace_hdl_t *dtp)
{
	sigaction(dtp->dt_proc_signal, &dtp->dt_proc_oact, NULL);
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
	 * dt_proc_create(), and only useful when dpr_created is true.
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
	struct sigevent sev = {0};
	int err;
	jmp_buf exec_jmp;

	/*
	 * Set up global libproc hooks that must be active before any processes
	 * are grabbed or created.
	 */
	Pset_ptrace_lock_hook(dt_proc_ptrace_lock);
	Pset_waitpid_lock_hook(dt_proc_waitpid_lock);
	Pset_libproc_unwinder_pad(dt_unwinder_pad);

	/*
	 * Arrange to clean up when cancelled by dt_proc_destroy() on shutdown.
	 */
	pthread_cleanup_push(dt_proc_control_cleanup, dpr);

	/*
	 * Lock our mutex, preventing races between cv broadcasts to our
	 * controlling thread and dt_proc_continue() or process destruction.
	 *
	 * It is eventually unlocked by dt_proc_control_cleanup(), and
	 * temporarily unlocked (while waiting) by Pwait(), called from
	 * dt_proc_loop().
	 */
	dt_proc_lock(dpr);

	/*
	 * Set up the machinery to allow the proxy thread to make requests of
	 * us: two ends of a pipe and one timer to signal this thread with the
	 * dt_proc_signal.  The timer is not yet armed.
	 */

	dpr->dpr_proxy_fd[0] = datap->dpcd_proxy_fd[0];
	dpr->dpr_proxy_fd[1] = datap->dpcd_proxy_fd[1];
	sev.sigev_notify = SIGEV_SIGNAL | SIGEV_THREAD_ID;
	sev.sigev_signo = dtp->dt_proc_signal;
	sev._sigev_un._tid = gettid();
	if (timer_create(CLOCK_MONOTONIC, &sev, &dpr->dpr_proxy_timer) < 0) {
		dt_proc_error(dtp, dpr, "failed to arm proxy timer for %i: %s\n",
			      dpr->dpr_pid, strerror(errno));
		pthread_exit(NULL);
	}

	/*
	 * Either create the process, or grab it.  Whichever, on failure, quit
	 * and let our cleanup run (signalling failure to
	 * dt_proc_create_thread() in the process).
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
		 * system daemons unless specifically demanded.  Also avoid
		 * ptracing if the process is already being traced by someone
		 * else (like another DTrace instance).  No death notification
		 * is ever sent.
		 *
		 * Also, obviously enough, never drop breakpoints in ourself!
		 */
		if (datap->dpcd_flags & DTRACE_PROC_SHORTLIVED) {
			pid_t tracer_pid, tgid;

			noninvasive = 1;
			dpr->dpr_notifiable = 0;
			tracer_pid = Ptracer_pid(dpr->dpr_pid);
			tgid = Ptgid(dpr->dpr_pid);

			if ((Psystem_daemon(dpr->dpr_pid, dtp->dt_useruid,
				    dtp->dt_sysslice) > 0) ||
			    (tracer_pid == getpid()) ||
			    (tgid == getpid()))
				noninvasive = 2;
		}

		if ((dpr->dpr_proc = Pgrab(dpr->dpr_pid, noninvasive, 0,
			    dpr, &err)) == NULL) {
			dt_proc_error(dtp, dpr, "failed to grab pid %li: %s\n",
			    (long)dpr->dpr_pid, strerror(err));
			pthread_exit(NULL);
		}

		/*
		 * If this was a noninvasive grab, quietly exit without calling
		 * the cleanup handlers: the process is running, but does not
		 * need a monitoring thread.
		 */
		if (!Ptraceable(dpr->dpr_proc)) {
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
			    (long)dpr->dpr_pid, strerror(err));
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
		 * Wait for a rendezvous from dt_proc_continue(), iff we were
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
	 * dt_proc_continue().  If the process exec()s after this point, this
	 * call is redundant, but harmless, and it saves setting up a new setjmp
	 * handler just to skip it.
	 *
	 * Then enter the main control loop.
	 */

	dt_proc_resume(dpr);
	dt_proc_loop(dpr, 0);

	/*
	 * If the caller is *still* waiting in dt_proc_continue() (i.e. the
	 * monitored process died before dtracing could start), resume it; then
	 * clean up.
	 */
	dt_proc_resume(dpr);
	pthread_cleanup_pop(1);

	return NULL;
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
	volatile struct pollfd pfd;
	int timeout = 0;
	int pwait_event_count;

	assert(MUTEX_HELD(&dpr->dpr_lock));

	/*
	 * Check the proxy pipe on every loop.
	 */

	pfd.fd = dpr->dpr_proxy_fd[0];
	pfd.events = POLLIN;

	/*
	 * If we're only proxying while waiting for a dt_proc_continue(), wait
	 * on it indefinitely; otherwise, don't wait, because we'll be waiting
	 * in Pwait() instead.
	 */
	if (awaiting_continue)
		timeout = -1;

	/*
	 * Check for any outstanding events, possibly sleeping to do so if we
	 * have no process to wait for.  Process any such events, then wait in
	 * Pwait() to handle any process events (again, unless we are
	 * awaiting_continue).  We want to sleep with dpr_lock unheld so that
	 * other parts of libdtrace can send requests to us, which is protected
	 * by that lock.  It is impossible for them, or any thread but this one,
	 * to modify the Pstate(), so we can call that without grabbing the
	 * lock.  We also unlock it around Pwait() so that proxy requests can
	 * initiate then.
	 */
	for (;;) {
		volatile int did_proxy_pwait = 0;

		dt_proc_unlock(dpr);

		while (errno = 0,
		    poll((struct pollfd *)&pfd, 1, timeout) <= 0 && errno == EINTR)
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
		 *
		 * Since we are about to process any proxy requests, we can
		 * clear the waitpid-interruption signal flag that sending a
		 * proxy request sets.  Note that while this is happening, the
		 * pinger can be hitting us with signals and setting
		 * waitpid_interrupted again!  That's fine: all a zero value
		 * indicates is that we do not know of any proxy requests
		 * waiting for us and trying to unblock waitpid(), not that
		 * there are none (one could just have started).
		 */
		dt_proc_lock(dpr);
		waitpid_interrupted = 0;

		/*
		 * Incoming proxy request.  Drain this byte out of the pipe, and
		 * handle it, with a new jmp_buf set up so as to redirect
		 * execve() detections back the calling thread.  (Multiple bytes
		 * cannot land on the pipe, so we don't need to consider this
		 * case -- but if they do, it is harmless, because the
		 * dpr_proxy_rq will be NULL in subsequent calls.)
		 */
		if (pfd.revents != 0) {
			char junk;
			jmp_buf this_exec_jmp, *old_exec_jmp;
			volatile int did_exec_retry = 0;

			while (read(dpr->dpr_proxy_fd[0], &junk, 1) < 0 && errno == EINTR);
			pfd.revents = 0;

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
				 * Pwait() from another thread.  Only one proxy
				 * request can be active at once, so thank
				 * goodness we don't need to worry about the
				 * possibility of another proxy request coming
				 * in while we're handling this one.
				 */
				if (dpr->dpr_proxy_rq == proxy_pwait) {
					dt_dprintf("%d: Handling a proxy Pwait(%i)\n",
					    dpr->dpr_pid,
					    dpr->dpr_proxy_args.dpr_pwait.block);
					errno = 0;
					dpr->dpr_proxy_ret = proxy_pwait
					    (dpr->dpr_proxy_args.dpr_pwait.P, dpr,
					         dpr->dpr_proxy_args.dpr_pwait.block,
						 NULL);

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
				 * execve() detected: the other thread requests
				 * that we reattach to the traced process, and
				 * set it going again.  On error, we terminate
				 * the process-control thread, because we no
				 * longer have anything to monitor.
				 */
				} else if (dpr->dpr_proxy_rq == proxy_reattach) {
					int err;

					dt_dprintf("%d: Handling a proxy_reattach()\n",
				 	    dpr->dpr_pid);
					errno = 0;
					dpr->dpr_proxy_ret = 0;
					err = dt_proc_reattach(dpr->dpr_hdl, dpr);
					if (err != 0)
						proxy_quit(dpr, err);
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
				} else if (dpr->dpr_proxy_rq == proxy_quit) {
					dt_dprintf("%d: handling a proxy_quit()\n",
					    dpr->dpr_pid);
					proxy_quit(dpr,
					    dpr->dpr_proxy_args.dpr_quit.err);
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

		if (awaiting_continue)
			continue;

		/*
		 * Pwait() for the process, listening for process state
		 * transitions, handling breakpoints and other problems,
		 * possibly detecting exec() and longjmping back out, etc.
		 *
		 * If a proxy request comes in, Pwait() returns 0.
		 *
		 * We do not unlock the dpr_lock at this stage because
		 * breakpoint invocations, proxied ptraces and the like can all
		 * require the lock to be held.  Instead, the waitpid_lock_hook
		 * unblocks it around the call to waitpid itself.
		 */

		dt_dprintf("%d: Waiting for process state changes\n",
			   dpr->dpr_pid);

		assert(MUTEX_HELD(&dpr->dpr_lock));
		pwait_event_count = Pwait(dpr->dpr_proc, B_TRUE, &waitpid_interrupted);

		if (pwait_event_count > 0) {
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
 *
 * Note that if it has quit on its own (perhaps due to process termination),
 * the main DTrace thread may be deep in libproc operations, and we must not
 * free or clean up things it might be using.  (Those operations will fail
 * with PS_DEAD and everything will unwind, but this does mean that we cannot
 * do Pfree() from this thread, even though it was this thread that did the
 * Pcreate()/Pgrab() in the first place.)
 *
 * We can assume that the process-control thread itself is at the
 * process-control loop or slightly before, though it may or may not hold the
 * dpr_lock.
 */
static void
dt_proc_control_cleanup(void *arg)
{
	int suiciding = 0;
	dt_proc_t *dpr = arg;
	pid_t pid;

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
	 * has exited, and notify any waiting thread that we have successfully
	 * exited.  Clean up the libproc state, unless this is a non-ptraceable
	 * process that doesn't need a monitor thread. (In that case, the
	 * monitor thread is suiciding but the libproc state should remain.)
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
		dt_proc_lock(dpr);

	/*
	 * Proxy cleanup.
	 *
	 * fd closing must be done with some care.  The thread may be cancelled
	 * before any of these fds have been assigned!
	 *
	 * No new incoming proxy calls are permitted after this point.  Flip
	 * dpr_done to ensure that none will be attempted, even if a proxyer is
	 * already blocked on the dpr_lock.  (However, this thread may still be
	 * in the midst of a proxy call, which is handled below.)
	 */

	dpr->dpr_done = B_TRUE;

	if (dpr->dpr_proxy_fd[0])
	    close(dpr->dpr_proxy_fd[0]);

	if (dpr->dpr_proxy_fd[1])
	    close(dpr->dpr_proxy_fd[1]);

	/*
	 * A proxy request may have come in since the last time we checked for
	 * one, before we took the lock, or we may be handling a proxy call such
	 * as dpr_quit(): abort any such request with a notice that the process
	 * is not there any more (though in fact it is; it will be gone by the
	 * time the dpr_lock is released.)
	 */
	dpr->dpr_proxy_errno = ESRCH;
	dpr->dpr_proxy_rq = NULL;
	pthread_cond_signal(&dpr->dpr_msg_cv);
	timer_delete(dpr->dpr_proxy_timer);

	/*
	 * Death-notification queueing is complicated by the fact that we might
	 * have died due to failure to create or grab a process in the first
	 * place, which means both that the dpr will not be queued into the dpr
	 * hash and that dt_proc_{grab,create}() will Pfree() it as soon as
	 * they notice that it's failed.  So we cannot enqueue the dpr in that
	 * case, and must enqueue a NULL instead.
	 *
	 * We also never want to emit a death notification for noninvasively-
	 * traced processes.
	 */
	if (!suiciding && dpr->dpr_notifiable)
		dt_proc_notify(dpr->dpr_hdl, dpr->dpr_hdl->dt_procs,
		    dpr->dpr_proc ? dpr : NULL, dpr->dpr_pid, NULL);

	/*
	 * Signal on the cv, waking up any waiters once the lock is released.
	 */
	pthread_cond_broadcast(&dpr->dpr_cv);

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
	 *
	 * The main thread may be blocked attempting to acquire the dpr_lock:
	 * in this case, proxy_call() detects our dpr_doneness and refuses
	 * to make further proxy calls.
	 */
        dpr->dpr_ending = 1;
	pid = dpr->dpr_pid;
	if (dpr->dpr_proc && !suiciding)
		Prelease(dpr->dpr_proc, dpr->dpr_created ? PS_RELEASE_KILL :
		    PS_RELEASE_NORMAL);

	if (dpr->dpr_lock_count_ctrl > 0) {
		dpr->dpr_lock_count_ctrl = 1;
		dt_proc_unlock(dpr);
	}

	/*
	 * After this point, no further dereferences of dpr from this thread are
	 * permitted.
	 *
	 * However, the control thread cannot be in both a Ptrace() and a
	 * condvar wait for cleanup simultaneously, so it is fine for the
	 * control thread to make references to dpr during Ptrace() unwinding,
	 * etc.
	 */
	dt_dprintf("%i: relinquished all locks.\n", pid);
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
	dt_proc_hash_t *dph = dtp->dt_procs;
	int err;

	/*
	 * Take out the dph_lock around this entire operation, to ensure that
	 * notifications cannot be called with a stale dprn_dpr->dpr_proc.
	 * (But after we are done, the dprn_dpr->dpr_proc is no longer stale, so
	 * we don't need to adjust outstanding notifications at all.
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
		    "after exec(): %s\n", (long)dpr->dpr_pid,
		    strerror(err));
		return err;
	}
	Ptrace_set_detached(dpr->dpr_proc, dpr->dpr_created);
	Puntrace(dpr->dpr_proc, 0);

	pthread_mutex_unlock(&dph->dph_lock);

	return 0;
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

static dt_proc_t *
dt_proc_lookup_remove(dtrace_hdl_t *dtp, pid_t pid, int remove)
{
	dt_proc_hash_t *dph = dtp->dt_procs;
	dt_proc_t *dpr, **dpp = &dph->dph_hash[pid & (dph->dph_hashlen - 1)];

	for (dpr = *dpp; dpr != NULL; dpr = dpr->dpr_hash) {
		if (dpr->dpr_pid == pid)
			break;
		else
			dpp = &dpr->dpr_hash;
	}

	if (remove) {
		assert(dpr != NULL);
		*dpp = dpr->dpr_hash; /* remove from pid hash chain */
	}

	return dpr;
}

dt_proc_t *
dt_proc_lookup(dtrace_hdl_t *dtp, pid_t pid)
{
	return dt_proc_lookup_remove(dtp, pid, 0);
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
	Pclose(P);
}

/*
 * Determine if a process is retired.  Very cheap.
 */
static int
dt_proc_retired(struct ps_prochandle *P)
{
	return !Phasfds(P);
}

/*
 * Destroy a dpr.  This is quite arcane due to avoiding races with the
 * process-control thread, which may be doing literally anything at the time
 * this is called, possibly many layers deep in self-proxy calls and breakpoint
 * insertion, most of which rely on the presence of the dpr in the dph, and all
 * of which rely on the dpr, ps_prochandle and associated machinery not having
 * been freed out from under it.  It also must be allowed to clean up neatly or
 * the victim will be left with outstanding (lethal) breakpoints.
 *
 * So the order of operations in this function, and dt_proc_control_cleanup(),
 * is crucial.
 */
static void
dt_proc_destroy(dtrace_hdl_t *dtp, dt_proc_t *dpr)
{
	ps_prochandle *P = dpr->dpr_proc;
	dt_proc_hash_t *dph = dtp->dt_procs;
	dt_proc_notify_t *npr;

	/*
	 * Remove this dt_proc_t from the lookup hash, and then walk the
	 * dt_proc_hash_t's notification list and remove this dt_proc_t if it is
	 * enqueued.  If the dpr is already gone, do nothing.
	 *
	 * Note that we do not actually free the dpr: the caller must do that.
	 * (This is because the caller may need the dpr to exist while it
	 * navigates to the next item on the list to delete.)
	 */
	if (P == NULL)
		return;

	dt_dprintf("%s pid %d\n", dpr->dpr_created ? "killing" : "releasing",
		dpr->dpr_pid);

	/*
	 * If the daemon thread is still alive, clean it up.
	 *
	 * Take out the lock around dpr_tid accesses, to ensure that we don't
	 * race with the setting of dpr_tid in dt_proc_control_cleanup().
	 *
	 * We must turn off background state change monitoring first,
	 * since cancellation triggers a libproc release, which flushes
	 * breakpoints and can wait on process state changes.
	 *
	 * We must do this before dt_proc_lookup_remove(), because the
	 * process-control threads may be issuing self-proxy operations, which
	 * though not going through a proxy_call(), still issue a
	 * dt_proc_lookup() to reacquire the dpr, and so require it to be in the
	 * dph hash.
	 */
	dt_proc_lock(dpr);
	proxy_monitor(dpr, 0);
	if (dpr->dpr_tid) {
		/*
		 * Cancel the daemon thread, then wait for dpr_done to indicate
		 * the thread has exited.  (This will also terminate the
		 * process.)
		 */
		proxy_quit(dpr, 0);
		dpr->dpr_lock_holder = pthread_self();
	} else {
		/*
		 * The process control thread is already dead, but try to clean
		 * the process up anyway, just in case it survived to this
		 * point.  This can happen e.g. if the process was noninvasively
		 * grabbed and its control thread suicided.)
		 */
		Prelease(dpr->dpr_proc, dpr->dpr_created ? PS_RELEASE_KILL :
		    PS_RELEASE_NORMAL);
	}
	dt_proc_unlock(dpr);

	/*
	 * Process-control thread gone: we can clean it off data structures
	 * and free it without fear of racing.
	 */
	pthread_mutex_lock(&dph->dph_lock);
	dt_proc_lookup_remove(dtp, dpr->dpr_pid, 1);
	npr = dph->dph_notify;

	while (npr != NULL) {
		if (npr->dprn_dpr == dpr)
			npr->dprn_dpr = NULL;
		npr = npr->dprn_next;
	}

	pthread_mutex_unlock(&dph->dph_lock);

	if (!dt_proc_retired(dpr->dpr_proc)) {
		assert(dph->dph_lrucnt != 0);
		dph->dph_lrucnt--;
	}
	dt_list_delete(&dph->dph_lrulist, dpr);
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

	pthread_mutex_lock(&dpr->dpr_lock);
	dpr->dpr_stop |= stop; /* set bit for initial rendezvous */
	dpr->dpr_monitoring = B_TRUE;
	if (flags & DTRACE_PROC_NOTIFIABLE)
		dpr->dpr_notifiable = 1;

	pthread_attr_init(&a);
	pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED);

	sigfillset(&nset);
	sigdelset(&nset, SIGABRT);	/* unblocked for assert() */
	sigdelset(&nset, dtp->dt_proc_signal);	/* unblocked for waitpid */

	data.dpcd_hdl = dtp;
	data.dpcd_proc = dpr;
	data.dpcd_start_proc = file;
	data.dpcd_start_proc_argv = argv;
	data.dpcd_flags = flags;

	if (pipe(data.dpcd_proxy_fd) < 0) {
		err = errno;
		dt_proc_error(dpr->dpr_hdl, dpr,
		    "failed to create communicating pipe for pid %d: %s\n",
		    (int)dpr->dpr_pid, strerror(err));

		pthread_mutex_unlock(&dpr->dpr_lock);
		pthread_attr_destroy(&a);
		return err;
	}

	pthread_sigmask(SIG_SETMASK, &nset, &oset);
	err = pthread_create(&dpr->dpr_tid, &a, dt_proc_control, &data);
	pthread_sigmask(SIG_SETMASK, &oset, NULL);

	/*
	 * If the control thread was created, then wait on dpr_cv for either
	 * dpr_done to be set (the victim died, the control thread failed, or no
	 * control thread was ultimately needed) or DT_PROC_STOP_IDLE to be set,
	 * indicating that the victim is now stopped and the control thread is
	 * at the rendezvous event.  On success, we return with the process and
	 * control thread stopped: the caller can then apply
	 * dt_proc_continue() to resume both.
	 */
	if (err == 0) {
		while (!dpr->dpr_done && !(dpr->dpr_stop & DT_PROC_STOP_IDLE))
			pthread_cond_wait(&dpr->dpr_cv, &dpr->dpr_lock);

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

	pthread_mutex_unlock(&dpr->dpr_lock);
	pthread_attr_destroy(&a);

	return err;
}

static dt_proc_t *
dt_proc_create(dtrace_hdl_t *dtp, const char *file, char *const *argv,
	int flags)
{
	dt_proc_hash_t *dph = dtp->dt_procs;
	dt_proc_t *dpr;
	pthread_mutexattr_t attr;
	pthread_mutexattr_t *attrp = NULL;

	if ((dpr = dt_zalloc(dtp, sizeof(dt_proc_t))) == NULL)
		return NULL; /* errno is set for us */

	if (_dtrace_debug_assert & DT_DEBUG_MUTEXES) {
		attrp = &attr;
		pthread_mutexattr_init(attrp);
		pthread_mutexattr_settype(attrp, PTHREAD_MUTEX_ERRORCHECK);
	}

	pthread_mutex_init(&dpr->dpr_lock, attrp);
	pthread_cond_init(&dpr->dpr_cv, NULL);
	pthread_cond_init(&dpr->dpr_msg_cv, NULL);

	if (_dtrace_debug_assert & DT_DEBUG_MUTEXES) {
		pthread_mutexattr_destroy(attrp);
		attrp = NULL;
	}

	/*
	 * Newly-created processes must be invasively grabbed.
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

		return NULL; /* dt_proc_error() has been called for us */
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
		dt_proc_continue(dtp, dpr);

	return dpr;
}

static dt_proc_t *
dt_proc_grab(dtrace_hdl_t *dtp, pid_t pid, int flags)
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
	 * ensure that dph_lrucnt and dt_proc_retired() do not get out of synch
	 * (which would cause aggressive early retirement of processes even when
	 * unnecessary).
	 *
	 * If it is noninvasive, or the process is dead, and the request was for
	 * an invasive grab, destroy it and make a new one (if the process is
	 * dead, this will obviously fail).  This destruction is safe because
	 * we know there is no control thread, so it is impossible for anything
	 * to be holding a reference to it.
	 */
	for (dpr = dph->dph_hash[h]; dpr != NULL;) {
		if ((dpr->dpr_pid == pid) &&
		    !(flags & DTRACE_PROC_SHORTLIVED) && !dpr->dpr_tid) {
				dt_dprintf("pid %d (cached, but noninvasive) "
				    "dropped.\n", (int)pid);

				dt_proc_t *npr = dpr->dpr_hash;

				dt_list_delete(&dph->dph_lrulist, dpr);
				dt_proc_destroy(dtp, dpr);
				dt_free(dtp, dpr);
				dpr = npr;

		} else if (dpr->dpr_pid == pid) {
			dt_dprintf("grabbed pid %d (cached)\n", (int)pid);

			dt_list_delete(&dph->dph_lrulist, dpr);
			dt_list_prepend(&dph->dph_lrulist, dpr);
			dpr->dpr_refs++;

			if (dt_proc_retired(dpr->dpr_proc)) {
				/* not retired any more */
				Pmemfd(dpr->dpr_proc);
				dph->dph_lrucnt++;
			}
			return dpr;
		}
		else
			dpr = dpr->dpr_hash;
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

	if ((dpr = dt_zalloc(dtp, sizeof(dt_proc_t))) == NULL)
		return NULL; /* errno is set for us */

	if (_dtrace_debug_assert & DT_DEBUG_MUTEXES) {
		attrp = &attr;
		pthread_mutexattr_init(attrp);
		pthread_mutexattr_settype(attrp, PTHREAD_MUTEX_ERRORCHECK);
	}

	pthread_mutex_init(&dpr->dpr_lock, attrp);
	pthread_cond_init(&dpr->dpr_cv, NULL);
	pthread_cond_init(&dpr->dpr_msg_cv, NULL);

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

		return NULL; /* dt_proc_error() has been called for us */
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
			if (opr->dpr_refs == 0 && !dt_proc_retired(opr->dpr_proc)) {
				dt_proc_retire(opr->dpr_proc);
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
		dt_proc_continue(dtp, dpr);

	return dpr;
}

static void
dt_proc_release(dtrace_hdl_t *dtp, dt_proc_t *dpr)
{
	dt_proc_hash_t *dph = dtp->dt_procs;

	assert(dpr != NULL);
	assert(dpr->dpr_refs != 0);

	if (--dpr->dpr_refs == 0 &&
	    (dph->dph_lrucnt > dph->dph_lrulim) &&
	    !dt_proc_retired(dpr->dpr_proc)) {
		dt_proc_retire(dpr->dpr_proc);
		dph->dph_lrucnt--;
	}

	if (dpr->dpr_done) {
		dt_proc_destroy(dtp, dpr);
		dt_free(dtp, dpr);
	}
}

static long
dt_proc_continue(dtrace_hdl_t *dtp, dt_proc_t *dpr)
{
	/*
	 * Noninvasively-grabbed processes are never stopped by us, so
	 * continuing them is meaningless.  The same is true of processes with
	 * dead control threads for whatever reason.
	 */
	if ((dpr->dpr_done) || (!dpr->dpr_proc) || (!dpr->dpr_tid) ||
	    (!Ptraceable(dpr->dpr_proc)))
		return 0;

	pthread_mutex_lock(&dpr->dpr_lock);

	dt_dprintf("%i: doing a dt_proc_continue().\n", dpr->dpr_pid);

	/*
	 * Calling dt_proc_continue() from the control thread is banned.
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
		pthread_kill(dpr->dpr_tid, dtp->dt_proc_signal);
		if (errno != 0 && errno != EINTR) {
			dt_proc_error(dpr->dpr_hdl, dpr, "Cannot write to "
			    "proxy pipe for dt_proc_continue(), deadlock is "
			    "certain: %s\n", strerror(errno));
			return -1;
		}
	}

	while (!dpr->dpr_done && !(dpr->dpr_stop & DT_PROC_STOP_RESUMED))
		pthread_cond_wait(&dpr->dpr_cv, &dpr->dpr_lock);

	dt_dprintf("%i: dt_proc_continue()d.\n", dpr->dpr_pid);

	pthread_mutex_unlock(&dpr->dpr_lock);

	return 0;
}

void
dt_proc_lock(dt_proc_t *dpr)
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

void
dt_proc_unlock(dt_proc_t *dpr)
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
	 * done under the dpr_lock, to stop dt_proc_destroy() from racing
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

/*
 * Take the lock around Ptrace() calls, to prevent other threads issuing
 * Ptrace()s while we are working.
 */
static void
dt_proc_ptrace_lock(struct ps_prochandle *P, void *arg, int ptracing)
{
	dt_proc_t *dpr = arg;

	if (ptracing)
		dt_proc_lock(dpr);
	else
		dt_proc_unlock(dpr);
}

/*
 * Release the lock around blocking waitpid() calls, so that proxy requests can
 * come in.  Proxy requests take the lock before hitting the process control
 * thread with a signal to wake it up: the lock is taken by the caller of the
 * various dt_Pfunction()s below, while proxy_monitor() invokes proxy_call()
 * which does the signalling.
 *
 * If we're shutting down, we don't do any of this: the proxy pipe is closed and
 * proxy requests cannot come in.  This hook is always called from the monitoring
 * thread, so the thread cannot transition from 'not shutting down' to 'shutting
 * down' within calls to this function, and we don't need to worry about
 * unbalanced dt_proc_unlock()/dt_proc_lock() calls.
 */
static void
dt_proc_waitpid_lock(struct ps_prochandle *P, void *arg, int waitpidding)
{
	dt_proc_t *dpr = arg;

	if (dpr->dpr_done)
		return;

	if (waitpidding)
		dt_proc_unlock(dpr);
	else {
		struct itimerspec nonpinger = {0};

                /*
		 * A waitpid() is done.  Disarm the signal-pinging timer
		 * immediately: the waitpid() has woken up, so its job is done.
		 */

		if (timer_settime(dpr->dpr_proxy_timer, 0, &nonpinger, NULL) < 0)
			dt_proc_error(dpr->dpr_hdl, dpr,
				      "Cannot disarm fallback wakeup timer: %s\n",
				      strerror(errno));

                dt_proc_lock(dpr);
	}
}

/*
 * Define the public interface to a libproc function from the rest of DTrace,
 * automatically proxying via the process-control thread and retrying on
 * exec().
 */
#define DEFINE_dt_Pfunction(function, err_ret, ...)		\
	dt_proc_t * volatile dpr = dt_proc_lookup(dtp, pid);	\
	jmp_buf this_exec_jmp, *old_exec_jmp; \
	\
	assert(MUTEX_HELD(&dpr->dpr_lock)); \
	old_exec_jmp = unwinder_pad; \
	if (setjmp(this_exec_jmp)) { \
		unwinder_pad = &this_exec_jmp; \
		if (!proxy_reattach(dpr)) \
			return err_ret; \
	} \
	unwinder_pad = &this_exec_jmp; \
	proxy_monitor(dpr, 0); \
	ret = function(dpr->dpr_proc, ## __VA_ARGS__); \
	proxy_monitor(dpr, 1); \
	unwinder_pad = old_exec_jmp;

int
dt_Plookup_by_addr(dtrace_hdl_t *dtp, pid_t pid, uintptr_t addr,
		   const char **sym_name, GElf_Sym *symbolp)
{
	int ret;
	DEFINE_dt_Pfunction(Plookup_by_addr, -1, addr, sym_name, symbolp);
	return ret;
}

const prmap_t *
dt_Paddr_to_map(dtrace_hdl_t *dtp, pid_t pid, uintptr_t addr)
{
	const prmap_t *ret;
	DEFINE_dt_Pfunction(Paddr_to_map, NULL, addr);
	return ret;
}

const prmap_t *
dt_Pname_to_map(dtrace_hdl_t *dtp, pid_t pid, const char *name)
{
	const prmap_t *ret;
	DEFINE_dt_Pfunction(Pname_to_map, NULL, name);
	return ret;
}

const prmap_t *
dt_Plmid_to_map(dtrace_hdl_t *dtp, pid_t pid, Lmid_t lmid, const char *name)
{
	const prmap_t *ret;
	DEFINE_dt_Pfunction(Plmid_to_map, NULL, lmid, name);
	return ret;
}

char *
dt_Pobjname(dtrace_hdl_t *dtp, pid_t pid, uintptr_t addr, char *buffer,
    size_t bufsize)
{
	char *ret;
	DEFINE_dt_Pfunction(Pobjname, NULL, addr, buffer, bufsize);
	return ret;
}

int
dt_Plmid(dtrace_hdl_t *dtp, pid_t pid, uintptr_t addr, Lmid_t *lmidp)
{
	int ret;
	DEFINE_dt_Pfunction(Plmid, -1, addr, lmidp);
	return ret;
}

int
dt_Pstate(dtrace_hdl_t *dtp, pid_t pid)
{
	int ret;
	DEFINE_dt_Pfunction(Pstate, -1);
	return ret;
}

int
dt_Pxlookup_by_name(dtrace_hdl_t *dtp, pid_t pid, Lmid_t lmid,
    const char *oname, const char *sname, GElf_Sym *symp, prsyminfo_t *sip)
{
	int ret;
	DEFINE_dt_Pfunction(Pxlookup_by_name, -1, lmid, oname, sname, symp,
	    sip);
	return ret;
}

int
dt_Pwritable_mapping(dtrace_hdl_t *dtp, pid_t pid, uintptr_t addr)
{
	int ret;
	DEFINE_dt_Pfunction(Pwritable_mapping, -1, addr);
	return ret;
}

int
dt_Psymbol_iter_by_addr(dtrace_hdl_t *dtp, pid_t pid, const char *object_name,
    int which, int mask, proc_sym_f *func, void *cd)
{
	int ret;
	DEFINE_dt_Pfunction(Psymbol_iter_by_addr, -1, object_name, which,
	    mask, func, cd);
	return ret;
}

int
dt_Pobject_iter(dtrace_hdl_t *dtp, pid_t pid, proc_map_f *func, void *cd)
{
	int ret;
	DEFINE_dt_Pfunction(Pobject_iter, -1, func, cd);
	return ret;
}

ssize_t
dt_Pread(dtrace_hdl_t *dtp, pid_t pid, void *buf, size_t nbyte,
    uintptr_t address)
{
	ssize_t ret;
	DEFINE_dt_Pfunction(Pread, -1, buf, nbyte, address);
	return ret;
}

void
dt_proc_hash_create(dtrace_hdl_t *dtp)
{
	if ((dtp->dt_procs = dt_zalloc(dtp, sizeof(dt_proc_hash_t) +
	    sizeof(dt_proc_t *) * _dtrace_pidbuckets - 1)) != NULL) {

		pthread_mutex_init(&dtp->dt_procs->dph_lock, NULL);

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
		dt_proc_destroy(dtp, dpr);
		dt_free(dtp, old_dpr);
		old_dpr = dpr;
	}
	dt_free(dtp, old_dpr);

	/*
	 * Wipe out the notification enqueues, since we will never need them
	 * again now DTrace is closing down.
	 */
	npp = &dph->dph_notify;
	while ((npr = *npp) != NULL) {
		*npp = npr->dprn_next;
		dt_free(dtp, npr);
	}

	dtp->dt_procs = NULL;
	dt_free(dtp, dph);
}

struct dtrace_proc *
dtrace_proc_create(dtrace_hdl_t *dtp, const char *file, char *const *argv,
	int flags)
{
	struct dtrace_proc *hdl;
	dt_ident_t *idp = dt_idhash_lookup(dtp->dt_macros, "target");
	dt_proc_t *dpr;

	hdl = malloc(sizeof(struct dtrace_proc));
	if (!hdl)
		return NULL;

	dpr = dt_proc_create(dtp, file, argv, flags | DTRACE_PROC_NOTIFIABLE);

	if (dpr == NULL) {
		free (hdl);
		return NULL;
	}

	hdl->pid = dpr->dpr_pid;
	if (idp != NULL && idp->di_id == 0)
		idp->di_id = hdl->pid; /* $target = created pid */
	return hdl;
}

struct dtrace_proc *
dtrace_proc_grab_pid(dtrace_hdl_t *dtp, pid_t pid, int flags)
{
	struct dtrace_proc *hdl;
	dt_ident_t *idp = dt_idhash_lookup(dtp->dt_macros, "target");
	dt_proc_t *dpr;

	hdl = malloc(sizeof(struct dtrace_proc));
	if (!hdl)
		return NULL;

	dpr = dt_proc_grab(dtp, pid, flags | DTRACE_PROC_NOTIFIABLE);
	if (dpr == NULL) {
		free (hdl);
		return NULL;
	}

	hdl->pid = dpr->dpr_pid;
	if (idp != NULL && idp->di_id == 0)
		idp->di_id = pid; /* $target = grabbed pid */

	return hdl;
}

pid_t
dtrace_proc_getpid(dtrace_hdl_t *dtp, struct dtrace_proc *proc)
{
	assert(proc != NULL);
	return proc->pid;
}

pid_t
dt_proc_grab_lock(dtrace_hdl_t *dtp, pid_t pid, int flags)
{
	dt_proc_t *dpr = dt_proc_grab(dtp, pid, flags);

	pid = -1;
	if (dpr != NULL) {
		dt_proc_lock(dpr);
		pid = dpr->dpr_pid;
	}

	return pid;
}

void
dt_proc_release_unlock(dtrace_hdl_t *dtp, pid_t pid)
{
	dt_proc_t *dpr = dt_proc_lookup(dtp, pid);

	if (dpr == NULL)
		return;

	dt_proc_unlock(dpr);
	dt_proc_release(dtp, dpr);
}

/*
 * Note: no proxying. Our tracking of the process is about to be destroyed: we
 * do not care if it exec()s.
 */
void
dtrace_proc_release(dtrace_hdl_t *dtp, struct dtrace_proc *proc)
{
	dt_proc_t *dpr;

	assert(proc != NULL);
	dpr = dt_proc_lookup(dtp, proc->pid);

	if (dpr != NULL)
		dt_proc_release(dtp, dpr);
	free(proc);
}

/*
 * Note: no proxying. The process is stopped and cannot exec().  (If it exec()ed
 * in the past, before it was stopped, a reattach will happen when the first
 * operation more significant than mere continuing takes place.)
 *
 * Must not be called from under a dt_proc_*_lock()!
 */
void
dtrace_proc_continue(dtrace_hdl_t *dtp, struct dtrace_proc *proc)
{
	dt_proc_t *dpr;

	assert(proc != NULL);
	dpr = dt_proc_lookup(dtp, proc->pid);

	if (dpr != NULL)
		dt_proc_continue(dtp, dpr);
}

/*
 * Set the internal signal number used to prod monitoring threads to wake up.
 */
int
dtrace_set_internal_signal(unsigned int sig)
{
	if (internal_proc_signal != -1) {
		dt_dprintf("Cannot change internal signal after DTrace is initialized.\n");
		return -1;
	}

        if (SIGRTMIN + sig > SIGRTMAX) {
		dt_dprintf("Internal signal %i+%i is greater than the maximum allowed, %i.\n",
			   SIGRTMIN, sig, SIGRTMAX);
		return -1;
	}

	internal_proc_signal = sig;
	return 0;
}
