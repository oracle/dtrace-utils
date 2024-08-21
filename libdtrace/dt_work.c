/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <stddef.h>
#include <errno.h>
#include <assert.h>
#include <time.h>
#include <libproc.h>
#include <port.h>
#include <linux/perf_event.h>
#include <sys/epoll.h>
#include <valgrind/valgrind.h>
#include <dt_impl.h>
#include <dt_aggregate.h>
#include <dt_peb.h>
#include <dt_probe.h>
#include <dt_bpf.h>
#include <dt_bpf_maps.h>
#include <dt_state.h>

void
BEGIN_probe(void)
{
}

void
END_probe(void)
{
}

int
dt_check_cpudrops(dtrace_hdl_t *dtp, processorid_t cpu, dtrace_dropkind_t what)
{
	dt_bpf_cpuinfo_t	*ci;
	uint32_t		cikey = 0;
	uint64_t		cnt;
	int			rval = 0;

	assert(what == DTRACEDROP_PRINCIPAL || what == DTRACEDROP_AGGREGATION);

	ci = dt_calloc(dtp, dtp->dt_conf.num_possible_cpus,
		       sizeof(dt_bpf_cpuinfo_t));
	if (ci == NULL)
		return dt_set_errno(dtp, EDT_NOMEM);

	if (dt_bpf_map_lookup(dtp->dt_cpumap_fd, &cikey, ci) == -1) {
		rval = dt_set_errno(dtp, EDT_BPF);
		goto fail;
	}

	if (what == DTRACEDROP_PRINCIPAL) {
		cnt = ci[cpu].buf_drops - dtp->dt_drops[cpu].buf;
		dtp->dt_drops[cpu].buf = ci[cpu].buf_drops;
	} else {
		cnt = ci[cpu].agg_drops - dtp->dt_drops[cpu].agg;
		dtp->dt_drops[cpu].agg = ci[cpu].agg_drops;
	}

	rval = dt_handle_cpudrop(dtp, cpu, what, cnt);

fail:
	dt_free(dtp, ci);
	return rval;
}

static void
dt_add_local_status(dtrace_hdl_t *dtp)
{
	/*
	 * We work on the most recently retrieved status, which is
	 * (dt_statusgen ^ 1) because the dt_get_status() function moves
	 * dt_statusgen after data retrieval *and* we get called after that
	 * data retrieval.
	 */
	dtrace_status_t	*st = &dtp->dt_status[dtp->dt_statusgen ^ 1];

	st->dtst_specdrops += dtp->dt_specdrops;
}

void
dt_get_status(dtrace_hdl_t *dtp)
{
	dtrace_status_t	*st = &dtp->dt_status[dtp->dt_statusgen];

	st->dtst_specdrops = dt_state_get(dtp, DT_STATE_SPEC_DROPS);
	st->dtst_specdrops_busy = dt_state_get(dtp, DT_STATE_SPEC_BUSY);
	st->dtst_specdrops_unavail = dt_state_get(dtp, DT_STATE_SPEC_UNAVAIL);
	st->dtst_dyndrops = dt_state_get(dtp, DT_STATE_DYNVAR_DROPS);
	dtp->dt_statusgen ^= 1;

	dt_add_local_status(dtp);
}

int
dtrace_status(dtrace_hdl_t *dtp)
{
	dtrace_optval_t	interval = dtp->dt_options[DTRACEOPT_STATUSRATE];
	hrtime_t	now = gethrtime();
	int		gen;

	if (!dtp->dt_active)
		return DTRACE_STATUS_NONE;

	if (dtp->dt_stopped)
		return DTRACE_STATUS_STOPPED;

	if (dtp->dt_laststatus != 0) {
		if (now - dtp->dt_laststatus < interval)
			return DTRACE_STATUS_NONE;

		dtp->dt_laststatus += interval;
	} else
		dtp->dt_laststatus = now;

	dt_get_status(dtp);
	gen = dtp->dt_statusgen;
	if (dt_handle_status(dtp, &dtp->dt_status[gen],
			     &dtp->dt_status[gen ^ 1]) == -1)
                return DTRACE_STATUS_ERROR;

	if (dt_state_get_activity(dtp) == DT_ACTIVITY_DRAINING) {
		if (!dtp->dt_stopped)
			dtrace_stop(dtp);

		return DTRACE_STATUS_EXITED;
	}

	return DTRACE_STATUS_OKAY;
}

#define CMD_BEGIN	1234
#define CMD_END		5678
typedef struct dt_beginendargs {
	pthread_t	thr;
	processorid_t	cpu;
	processorid_t	ncpus;
	int	tochild[2];
	int	frchild[2];
} dt_beginendargs_t;

static
void bind_to_cpu(int cpu, int ncpus) {
	cpu_set_t *mask;
	size_t size;

	/* Allocate the CPU mask and get its size. */
	mask = CPU_ALLOC(ncpus);
	if (mask == NULL)
		exit(1);
	size = CPU_ALLOC_SIZE(ncpus);

	/* Set the CPU mask. */
	CPU_ZERO_S(size, mask);
	CPU_SET_S(cpu, size, mask);

	/* Set my affinity. */
	if (sched_setaffinity(0, size, mask) != 0) {
		/* FIXME: some other failure mode? */
		exit(1);
	}

	/* Free the mask. */
	CPU_FREE(mask);
}

static unsigned long long
elapsed_msecs() {
	struct timespec tstruct;

	clock_gettime(CLOCK_MONOTONIC, &tstruct);
	return tstruct.tv_sec * 1000ull + tstruct.tv_nsec / 1000000;
}

static void *
beginend_child(void *arg) {
	dt_beginendargs_t *args = arg;
	int cmd = 0;

	/* Bind to requested CPU. */
	bind_to_cpu(args->cpu, args->ncpus);

	/* Wait for command, call BEGIN_probe(), and ack. */
	read(args->tochild[0], &cmd, sizeof(cmd));
	if (cmd != CMD_BEGIN)
		exit(1);
	if (RUNNING_ON_VALGRIND)
		VALGRIND_NON_SIMD_CALL0(BEGIN_probe);
	else
		BEGIN_probe();
	cmd++;
	write(args->frchild[1], &cmd, sizeof(cmd));

	/* Wait for command, call END_probe(), and ack. */
	read(args->tochild[0], &cmd, sizeof(cmd));
	if (cmd != CMD_END)
		exit(1);
	if (RUNNING_ON_VALGRIND)
		VALGRIND_NON_SIMD_CALL0(END_probe);
	else
		END_probe();
	cmd++;
	write(args->frchild[1], &cmd, sizeof(cmd));

	pthread_exit(0);
}

int
dtrace_go(dtrace_hdl_t *dtp, uint_t cflags)
{
	size_t			size;
	struct epoll_event	ev;

	if (dtp->dt_active)
		return dt_set_errno(dtp, EINVAL);

	/*
	 * Create a child for the BEGIN and END probes if -xcpu is used.
	 */
	if (dtp->dt_options[DTRACEOPT_CPU] != DTRACEOPT_UNSET) {
		dt_beginendargs_t	*args;

		args = dt_zalloc(dtp, sizeof(dt_beginendargs_t));
		if (args == NULL)
			return dt_set_errno(dtp, EDT_NOMEM);
		pipe(args->tochild);
		pipe2(args->frchild, O_NONBLOCK);
		args->cpu = dtp->dt_options[DTRACEOPT_CPU];
		args->ncpus = dtp->dt_conf.max_cpuid + 1;

		if (pthread_create(&args->thr, NULL, &beginend_child, args)) {
			printf("error pthread_create for -xcpu\n");
			return -1;
		}
		dtp->dt_beginendargs = args;
	}

	/* Create the BPF programs. */
	if (dt_bpf_make_progs(dtp, cflags) == -1)
		return -1;

	/* Create the global BPF maps. */
	if (dt_bpf_gmap_create(dtp) == -1)
		return -1;

	/* Load the BPF programs. */
	if (dt_bpf_load_progs(dtp, cflags) == -1)
		return -1;

	/*
	 * Set up the event polling file descriptor.
	 */
	dtp->dt_poll_fd = epoll_create1(EPOLL_CLOEXEC);
	if (dtp->dt_poll_fd < 0)
		return dt_set_errno(dtp, errno);

	/*
	 * Register the proc eventfd descriptor to receive notifications about
	 * process exit.
	 */
	ev.events = EPOLLIN;
	ev.data.ptr = dtp->dt_procs;
	if (epoll_ctl(dtp->dt_poll_fd, EPOLL_CTL_ADD, dtp->dt_proc_fd, &ev) ==
	    -1)
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

	/*
	 * We must initialize the aggregation consumer handling before we
	 * trigger the BEGIN probe.
	 */
	if (dt_aggregate_go(dtp) == -1)
		return -1;

	if (dtp->dt_beginendargs) {
		/* Tell child running on a specific CPU to BEGIN. */
		dt_beginendargs_t	*args = dtp->dt_beginendargs;
		unsigned long long	timeout;
		int			cmd = CMD_BEGIN;

		write(args->tochild[1], &cmd, sizeof(cmd));
		timeout = elapsed_msecs() + 2000;
		while (read(args->frchild[0], &cmd, sizeof(cmd)) <= 0) {
			usleep(100000);
			if (elapsed_msecs() > timeout)
				return -1;
		}
		if (cmd != CMD_BEGIN + 1)
			return -1;
	} else if (RUNNING_ON_VALGRIND)
		VALGRIND_NON_SIMD_CALL0(BEGIN_probe);
	else
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

	return 0;
}

int
dtrace_stop(dtrace_hdl_t *dtp)
{
	if (dtp->dt_stopped)
		return 0;

	if (dt_state_get_activity(dtp) < DT_ACTIVITY_DRAINING)
		dt_state_set_activity(dtp, DT_ACTIVITY_DRAINING);

	if (dtp->dt_beginendargs) {
		int			cmd = CMD_END;
		dt_beginendargs_t	*args = dtp->dt_beginendargs;
		unsigned long long	timeout;

		write(args->tochild[1], &cmd, sizeof(cmd));
		timeout = elapsed_msecs() + 2000;
		while (read(args->frchild[0], &cmd, sizeof(cmd)) <= 0) {
			usleep(100000);
			if (elapsed_msecs() > timeout)
				return -1;
		}
		if (cmd != CMD_END + 1)
			return -1;
		pthread_join(args->thr, NULL);
		dt_free(dtp, args);
	} else if (RUNNING_ON_VALGRIND)
		VALGRIND_NON_SIMD_CALL0(END_probe);
	else
		END_probe();

	dtp->dt_stopped = 1;
	dtp->dt_endedon = dt_state_get_endedon(dtp);

	return 0;
}

dtrace_workstatus_t
dtrace_work(dtrace_hdl_t *dtp, FILE *fp, dtrace_consume_probe_f *pfunc,
	    dtrace_consume_rec_f *rfunc, void *arg)
{
	dtrace_workstatus_t	rval;
	int			gen;

	switch (dtrace_status(dtp)) {
	case DTRACE_STATUS_EXITED:
	case DTRACE_STATUS_STOPPED:
		dtp->dt_lastswitch = 0;
		dtp->dt_lastagg = 0;
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

	/*
	 * If we are not stopped, we use dt_add_local_status() to adjust the
	 * current drop counters without retrieving producer counts (since they
	 * might have changed and we do not want to report them yet).
	 *
	 * If we are stopped, we use dt_get_status() to get any potential
	 * pending speculation drops because we want to ensure that old and new
	 * counts for other drops are identical (lest they be reported more
	 * than once).
	 */
	if (!dtp->dt_stopped) {
		gen = dtp->dt_statusgen;
		dtp->dt_status[gen] = dtp->dt_status[gen ^ 1];
		dt_add_local_status(dtp);
		gen = dtp->dt_statusgen ^ 1;
	} else {
		dt_get_status(dtp);
		gen = dtp->dt_statusgen;
	}

	dt_handle_status(dtp, &dtp->dt_status[gen], &dtp->dt_status[gen ^ 1]);

	return rval;
}
