/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <dt_impl.h>
#include <dt_peb.h>
#include <dt_probe.h>
#include <dt_bpf.h>
#include <dt_state.h>
#include <stddef.h>
#include <errno.h>
#include <assert.h>
#include <time.h>
#include <libproc.h>
#include <port.h>
#include <linux/perf_event.h>
#include <sys/epoll.h>

static const struct {
	int dtslt_option;
	size_t dtslt_offs;
} _dtrace_sleeptab[] = {
	{ DTRACEOPT_STATUSRATE, offsetof(dtrace_hdl_t, dt_laststatus) },
	{ DTRACEOPT_AGGRATE, offsetof(dtrace_hdl_t, dt_lastagg) },
	{ DTRACEOPT_SWITCHRATE, offsetof(dtrace_hdl_t, dt_lastswitch) },
	{ DTRACEOPT_MAX, 0 }
};

void
BEGIN_probe(void)
{
}

void
END_probe(void)
{
}

void
dtrace_sleep(dtrace_hdl_t *dtp)
{
	dt_proc_hash_t *dph = dtp->dt_procs;
	dtrace_optval_t policy = dtp->dt_options[DTRACEOPT_BUFPOLICY];
	dt_proc_notify_t *dprn;

	hrtime_t earliest = INT64_MAX;
	struct timespec tv;
	int i;

	for (i = 0; _dtrace_sleeptab[i].dtslt_option < DTRACEOPT_MAX; i++) {
		uintptr_t a = (uintptr_t)dtp + _dtrace_sleeptab[i].dtslt_offs;
		int opt = _dtrace_sleeptab[i].dtslt_option;
		dtrace_optval_t interval = dtp->dt_options[opt];

		/*
		 * If the buffering policy is set to anything other than
		 * "switch", we ignore the aggrate and switchrate -- they're
		 * meaningless.
		 */
		if (policy != DTRACEOPT_BUFPOLICY_SWITCH &&
		    _dtrace_sleeptab[i].dtslt_option != DTRACEOPT_STATUSRATE)
			continue;

		if (*((hrtime_t *)a) + interval < earliest)
			earliest = *((hrtime_t *)a) + interval;
	}

	(void) pthread_mutex_lock(&dph->dph_lock);

	tv.tv_sec = earliest / NANOSEC;
	tv.tv_nsec = earliest % NANOSEC;

	/*
	 * Wait until the time specified by "earliest" has arrived, or until we
	 * receive notification that a process is in an interesting state; also
	 * make sure that any synchronous notifications of process exit are
	 * received.  Regardless of why we awaken, iterate over any pending
	 * notifications and process them.
	 */
	(void) pthread_cond_timedwait(&dph->dph_cv, &dph->dph_lock, &tv);
	(void) dt_proc_enqueue_exits(dtp);

	while ((dprn = dph->dph_notify) != NULL) {
		if (dtp->dt_prochdlr != NULL) {
			char *err = dprn->dprn_errmsg;
			pid_t pid = dprn->dprn_pid;
			int state = PS_DEAD;

			/*
			 * The dprn_dpr may be NULL if attachment or process
			 * creation has failed, or once the process dies.  Only
			 * get the state of a dprn that is not NULL.
			 */
			if (dprn->dprn_dpr != NULL) {
				pid = dprn->dprn_dpr->dpr_pid;
				dt_proc_lock(dprn->dprn_dpr);
			}

			if (*err == '\0')
				err = NULL;

			if (dprn->dprn_dpr != NULL)
				state = dt_Pstate(dtp, pid);

			if (state < 0 || state == PS_DEAD)
				pid *= -1;

			if (dprn->dprn_dpr != NULL)
				dt_proc_unlock(dprn->dprn_dpr);

			dtp->dt_prochdlr(pid, err, dtp->dt_procarg);
		}

		dph->dph_notify = dprn->dprn_next;
		dt_free(dtp, dprn);
	}

	(void) pthread_mutex_unlock(&dph->dph_lock);
}

int
dtrace_status(dtrace_hdl_t *dtp)
{
	if (!dtp->dt_active)
		return DTRACE_STATUS_NONE;

	if (dtp->dt_stopped)
		return DTRACE_STATUS_STOPPED;

	if (dt_state_get_activity(dtp) == DT_ACTIVITY_DRAINING) {
		if (!dtp->dt_stopped)
			dtrace_stop(dtp);

		return DTRACE_STATUS_EXITED;
	}

	return DTRACE_STATUS_OKAY;
}

int
dtrace_go(dtrace_hdl_t *dtp, uint_t cflags)
{
	void		*dof;
	size_t		size;
	int		err;

	if (dtp->dt_active)
		return (dt_set_errno(dtp, EINVAL));

	/*
	 * If a dtrace:::ERROR program and callback are registered, enable the
	 * program before we start tracing.  If this fails for a vector open
	 * with ENOTTY, we permit dtrace_go() to succeed so that vector clients
	 * such as mdb's dtrace module can execute the rest of dtrace_go() even
	 * though they do not provide support for the DTRACEIOC_ENABLE ioctl.
	 */
	if (dtp->dt_errprog != NULL &&
	    dtrace_program_exec(dtp, dtp->dt_errprog, NULL) == -1 && (
	    dtp->dt_errno != ENOTTY || dtp->dt_vector == NULL))
		return (-1); /* dt_errno has been set for us */

	/*
	 * Create the global BPF maps.  This is done only once regardless of
	 * how many programs there are.
	 */
	err = dt_bpf_gmap_create(dtp);
	if (err)
		return err;

	err = dt_bpf_load_progs(dtp, cflags);
	if (err)
		return err;

	/*
	 * Set up the event polling file descriptor.
	 */
	dtp->dt_poll_fd = epoll_create1(EPOLL_CLOEXEC);
	if (dtp->dt_poll_fd < 0)
		return dt_set_errno(dtp, errno);

	/*
	 * We need enough space for the pref_event_header, a 32-bit size, a
	 * 4-byte gap, and the largest trace data record we may be writing to
	 * the buffer.  In other words, the buffer needs to be large enough to
	 * hold at least one perf-encapsulated trace data record.
	 */
	dtrace_getopt(dtp, "bufsize", &size);
	if (size == 0 ||
	    size < sizeof(struct perf_event_header) + sizeof(uint32_t) +
		   dtp->dt_maxreclen)
		return dt_set_errno(dtp, EDT_BUFTOOSMALL);
	if (dt_pebs_init(dtp, size) == -1)
		return dt_set_errno(dtp, EDT_NOMEM);

	BEGIN_probe();

	dtp->dt_active = 1;
	dtp->dt_beganon = dt_state_get_beganon(dtp);

	/*
	 * An exit() action during the BEGIN probe processing will cause the
	 * activity state to become STOPPED once the BEGIN probe is done.  We
	 * need to move it back to DRAINING in that case.
	 */
	if (dt_state_get_activity(dtp) == DT_ACTIVITY_STOPPED)
		dt_state_set_activity(dtp, DT_ACTIVITY_DRAINING);

#if 0
	if (dt_options_load(dtp) == -1)
		return (dt_set_errno(dtp, errno));

	return (dt_aggregate_go(dtp));
#else
	return 0;
#endif
}

int
dtrace_stop(dtrace_hdl_t *dtp)
{
	int		gen = dtp->dt_statusgen;

	if (dtp->dt_stopped)
		return 0;

	END_probe();

	dtp->dt_stopped = 1;
	dtp->dt_endedon = dt_state_get_endedon(dtp);

#if 0
	/*
	 * Now that we're stopped, we're going to get status one final time.
	 */
	if (dt_ioctl(dtp, DTRACEIOC_STATUS, &dtp->dt_status[gen]) == -1)
		return (dt_set_errno(dtp, errno));
#endif

	if (dt_handle_status(dtp, &dtp->dt_status[gen ^ 1],
	    &dtp->dt_status[gen]) == -1)
		return -1;

	return 0;
}

#if 0
dtrace_workstatus_t
dtrace_work(dtrace_hdl_t *dtp, FILE *fp,
    dtrace_consume_probe_f *pfunc, dtrace_consume_rec_f *rfunc, void *arg)
{
	int status = dtrace_status(dtp);
	dtrace_optval_t policy = dtp->dt_options[DTRACEOPT_BUFPOLICY];
	dtrace_workstatus_t rval;

	switch (status) {
	case DTRACE_STATUS_EXITED:
	case DTRACE_STATUS_FILLED:
	case DTRACE_STATUS_STOPPED:
		/*
		 * Tracing is stopped.  We now want to force dtrace_consume()
		 * and dtrace_aggregate_snap() to proceed, regardless of
		 * switchrate and aggrate.  We do this by clearing the times.
		 */
		dtp->dt_lastswitch = 0;
		dtp->dt_lastagg = 0;
		rval = DTRACE_WORKSTATUS_DONE;
		break;

	case DTRACE_STATUS_NONE:
	case DTRACE_STATUS_OKAY:
		rval = DTRACE_WORKSTATUS_OKAY;
		break;

	default:
		return (DTRACE_WORKSTATUS_ERROR);
	}

	if ((status == DTRACE_STATUS_NONE || status == DTRACE_STATUS_OKAY) &&
	    policy != DTRACEOPT_BUFPOLICY_SWITCH) {
		/*
		 * There either isn't any status or things are fine -- and
		 * this is a "ring" or "fill" buffer.  We don't want to consume
		 * any of the trace data or snapshot the aggregations; we just
		 * return.
		 */
		assert(rval == DTRACE_WORKSTATUS_OKAY);
		return (rval);
	}

#if 0
	if (dtrace_aggregate_snap(dtp) == -1)
		return (DTRACE_WORKSTATUS_ERROR);
#endif

	if (dtrace_consume(dtp, fp, pfunc, rfunc, arg) == -1)
		return (DTRACE_WORKSTATUS_ERROR);

	return (rval);
}
#else
dtrace_workstatus_t
dtrace_work(dtrace_hdl_t *dtp, FILE *fp, dtrace_consume_probe_f *pfunc,
	    dtrace_consume_rec_f *rfunc, void *arg)
{
	int			status = dtrace_status(dtp);
	dtrace_workstatus_t	rval;

	switch (dtrace_status(dtp)) {
	case DTRACE_STATUS_EXITED:
	case DTRACE_STATUS_STOPPED:
		rval = DTRACE_WORKSTATUS_DONE;
		break;
	case DTRACE_STATUS_NONE:
	case DTRACE_STATUS_OKAY:
		rval = DTRACE_WORKSTATUS_OKAY;
		break;
	default:
		return DTRACE_WORKSTATUS_ERROR;
	}

	if (dtrace_consume(dtp, fp, pfunc, rfunc, arg) ==
	    DTRACE_WORKSTATUS_ERROR)
		return DTRACE_WORKSTATUS_ERROR;

	return rval;
}
#endif
