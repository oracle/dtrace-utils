/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * The profile provider for DTrace.
 */
#include <assert.h>
#include <sys/ioctl.h>

#include <bpf_asm.h>

#include "dt_dctx.h"
#include "dt_cg.h"
#include "dt_bpf.h"
#include "dt_probe.h"

static const char		prvname[] = "profile";
static const char		modname[] = "";
static const char		funname[] = "";

#define KIND_PROFILE	0
#define KIND_TICK	1
#define PREFIX_PROFILE	"profile-"
#define PREFIX_TICK	"tick-"

static const dtrace_pattr_t	pattr = {
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_UNSTABLE, DTRACE_STABILITY_UNSTABLE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
};

#define FDS_CNT(kind)	((kind) == KIND_TICK ? 1 : dtp->dt_conf.num_online_cpus)
typedef struct profile_probe {
	int		kind;
	uint64_t	period;
	int		*fds;
} profile_probe_t;

static dt_probe_t *profile_probe_insert(dtrace_hdl_t *dtp, dt_provider_t *prv,
				        const char *prb, int kind,
					uint64_t period)
{
	profile_probe_t	*pp;
	int		i;
	int		cnt = FDS_CNT(kind);

	pp = dt_zalloc(dtp, sizeof(profile_probe_t));
	if (pp == NULL)
		return NULL;

	pp->kind = kind;
	pp->period = period;
	pp->fds = dt_calloc(dtp, cnt, sizeof(int));
	if (pp->fds == NULL)
		goto err;

	for (i = 0; i < cnt; i++)
		pp->fds[i] = -1;

	return dt_probe_insert(dtp, prv, prvname, modname, funname, prb, pp);

err:
	dt_free(dtp, pp);
	return NULL;
}

static int populate(dtrace_hdl_t *dtp)
{
	dt_provider_t	*prv;
	char		buf[32];
	int		i, n = 0;
	int		profile_n[] = { 97, 199, 499, 997, 1999, 4001, 4999 };
	int		tick_n[] = { 1, 10, 100, 500, 1000, 5000 };

	prv = dt_provider_create(dtp, prvname, &dt_profile, &pattr);
	if (prv == NULL)
		return 0;

	for (i = 0; i < ARRAY_SIZE(profile_n); i++) {
		snprintf(buf, sizeof(buf), PREFIX_PROFILE "%d", profile_n[i]);
		if (profile_probe_insert(dtp, prv, buf, KIND_PROFILE,
					 1000000000ul / profile_n[i]))
			n++;
	}

	for (i = 0; i < ARRAY_SIZE(tick_n); i++) {
		snprintf(buf, sizeof(buf), PREFIX_TICK "%d", tick_n[i]);
		if (profile_probe_insert(dtp, prv, buf, KIND_TICK,
					 1000000000ul / tick_n[i]))
			n++;
	}

	return n;
}

static uint64_t get_period(const char *name)
{
	char		*p;
	char		suffix[5] = "hz";	/* default is frequency */
	int		i;
	uint64_t	val = 0;

	const struct {
		char		*suffix;
		uint64_t	mult;
	} suffixes[] = {
			{ "ns",				 1ul },
			{ "nsec",			 1ul },
			{ "us",			      1000ul },
			{ "usec",		      1000ul },
			{ "ms",			   1000000ul },
			{ "msec",		   1000000ul },
			{ "s",			1000000000ul },
			{ "sec",		1000000000ul },
			{ "m",		   60 * 1000000000ul },
			{ "min",	   60 * 1000000000ul },
			{ "h",	      60 * 60 * 1000000000ul },
			{ "hour",     60 * 60 * 1000000000ul },
			{ "d",	 24 * 60 * 60 * 1000000000ul },
			{ "day", 24 * 60 * 60 * 1000000000ul },
			{ NULL, },
		       };

	p = strchr(name, '-') + 1;
	switch (sscanf(p, "%lu%4s", &val, suffix)) {
	default:		/* no match -> fail */
		return 0;
	case 1:			/* no suffix -> default is hz */
	case 2:			/* apply suffix */
		if (val == 0)
			return 0;
		if (strcasecmp("hz", suffix) == 0) {
			val = 1000000000ul / val;
			break;
		}

		for (i = 0; suffixes[i].suffix != NULL; i++) {
			if (strcasecmp(suffixes[i].suffix, suffix) == 0) {
				val *= suffixes[i].mult;
				break;
			}
		}

		if (suffixes[i].suffix == NULL)
			return 0;
	}

	/* enforce the 200-usec limit */
	if (val < 200000)
		return 0;

	return val;
}

static int provide(dtrace_hdl_t *dtp, const dtrace_probedesc_t *pdp)
{
	dt_provider_t	*prv;
	int		kind;
	uint64_t	period;

	/* make sure we have IDNONE and a legal name */
	if (pdp->id != DTRACE_IDNONE || strcmp(pdp->prv, prvname) ||
	    strcmp(pdp->mod, modname) || strcmp(pdp->fun, funname))
		return 0;

	if (strncmp(pdp->prb, PREFIX_PROFILE, strlen(PREFIX_PROFILE)) == 0)
		kind = KIND_PROFILE;
	else if (strncmp(pdp->prb, PREFIX_TICK, strlen(PREFIX_TICK)) == 0)
		kind = KIND_TICK;
	else
		return 0;

	/* return if we already have this probe */
	if (dt_probe_lookup(dtp, pdp))
		return 0;

	/* get the provider - should have been created in populate() */
	prv = dt_provider_lookup(dtp, prvname);
	if (!prv)
		return 0;

	period = get_period(pdp->prb);
	if (period == 0)
		return 0;

	/* try to add this probe */
	if (profile_probe_insert(dtp, prv, pdp->prb, kind, period) == NULL)
		return 0;

	return 1;
}

/*
 * Generate a BPF trampoline for a profile probe.
 *
 * The trampoline function is called when a profile probe triggers, and it must
 * satisfy the following prototype:
 *
 *	int dt_profile(struct bpf_perf_event_data *ctx)
 *
 * The trampoline will populate a dt_bpf_context struct and then call the
 * function that implements the compiled D clause.  It returns the value that
 * it gets back from that function.
 *
 * The context that is passed to the trampoline is:
 *     struct bpf_perf_event_data {
 *         bpf_user_pt_regs_t regs;
 *         __u64 sample_period;
 *         __u64 addr;
 *     }
 */
static void trampoline(dt_pcb_t *pcb)
{
	int		i;
	dt_irlist_t	*dlp = &pcb->pcb_ir;

	dt_cg_tramp_prologue(pcb);

	/*
	 * After the dt_cg_tramp_prologue() call, we have:
	 *				//     (%r7 = dctx->mst)
	 *				//     (%r8 = dctx->ctx)
	 */

	dt_cg_tramp_copy_regs(pcb);

	/*
	 * TODO:
	 * For profile-n probes:
	 *     dctx->mst->argv[0] = kernel PC
	 *     dctx->mst->argv[1] = userspace PC
	 *     dctx->mst->argv[2] = elapsed nsecs
	 *
	 * For tick-n probes:
	 *     dctx->mst->argv[0] = kernel PC
	 *     dctx->mst->argv[1] = userspace PC
	 *
	 * For now, we can only provide the first argument:
	 *     dctx->mst->argv[0] = PT_REGS_IP((dt_pt_regs *)&dctx->ctx->regs);
	 *                              //  lddw %r0, [%r8 + PT_REGS_IP]
	 *                              //  stdw [%r7 + DMST_ARG(0)], %r0
	 */
	emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_8, PT_REGS_IP));
	emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(0), BPF_REG_0));

	/*
	 *     (we clear dctx->mst->argv[1] and on)
	 */
	for (i = 1; i < ARRAY_SIZE(((dt_mstate_t *)0)->argv); i++)
		emit(dlp, BPF_STORE_IMM(BPF_DW, BPF_REG_7, DMST_ARG(i), 0));

	dt_cg_tramp_epilogue(pcb);
}

static int attach(dtrace_hdl_t *dtp, const dt_probe_t *prp, int bpf_fd)
{
	profile_probe_t		*pp = prp->prv_data;
	struct perf_event_attr	attr;
	int			i, nattach = 0;;
	int			cnt = FDS_CNT(pp->kind);

	memset(&attr, 0, sizeof(attr));
	attr.type = PERF_TYPE_SOFTWARE;
	attr.config = PERF_COUNT_SW_CPU_CLOCK;
	attr.size = sizeof(attr);
	attr.wakeup_events = 1;
	attr.freq = 0;
	attr.sample_period = pp->period;

	for (i = 0; i < cnt; i++) {
		int j = i, fd;

		/* if there is only one fd, place it at random */
		if (cnt == 1)
			j = rand() % dtp->dt_conf.num_online_cpus;

		fd = perf_event_open(&attr, -1, dtp->dt_conf.cpus[j].cpu_id,
				     -1, 0);
		if (fd < 0)
			continue;
		if (ioctl(fd, PERF_EVENT_IOC_SET_BPF, bpf_fd) < 0) {
			close(fd);
			continue;
		}
		pp->fds[i] = fd;
		nattach++;
	}

	return nattach > 0 ? 0 : -1;
}

static int probe_info(dtrace_hdl_t *dtp, const dt_probe_t *prp,
		      int *argcp, dt_argdesc_t **argvp)
{
	/* profile-provider probe arguments are not typed */
	*argcp = 0;
	*argvp = NULL;

	return 0;
}

static void detach(dtrace_hdl_t *dtp, const dt_probe_t *prp)
{
	profile_probe_t	*pp = prp->prv_data;
	int		i;
	int		cnt = FDS_CNT(pp->kind);

	for (i = 0; i < cnt; i++) {
		if (pp->fds[i] != -1)
			close(pp->fds[i]);
	}
}

static void probe_destroy(dtrace_hdl_t *dtp, void *arg)
{
	profile_probe_t	*pp = arg;

	dt_free(dtp, pp->fds);
	dt_free(dtp, pp);
}

dt_provimpl_t	dt_profile = {
	.name		= prvname,
	.prog_type	= BPF_PROG_TYPE_PERF_EVENT,
	.populate	= &populate,
	.provide	= &provide,
	.trampoline	= &trampoline,
	.attach		= &attach,
	.probe_info	= &probe_info,
	.detach		= &detach,
	.probe_destroy	= &probe_destroy,
};
