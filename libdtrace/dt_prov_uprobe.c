/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * The uprobe-based provider for DTrace (implementing pid and USDT providers).
 */
#include <sys/types.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#include <bpf_asm.h>

#include "dt_dctx.h"
#include "dt_cg.h"
#include "dt_list.h"
#include "dt_provider.h"
#include "dt_probe.h"
#include "dt_pid.h"
#include "dt_string.h"
#include "uprobes.h"

/* Provider name for the underlying probes. */
static const char		prvname[] = "uprobe";

#define UPROBE_EVENTS		TRACEFS "uprobe_events"

#define PP_IS_MINE	1
#define PP_IS_RETURN	2
#define PP_IS_FUNCALL	4

typedef struct dt_uprobe {
	dev_t		dev;
	ino_t		inum;
	char		*fn;
	uint64_t	off;
	int		flags;
	tp_probe_t	*tp;
	dt_list_t	probes;		/* pid/USDT probes triggered by it */
} dt_uprobe_t;

typedef struct list_probe {
	dt_list_t	list;
	dt_probe_t	*probe;
} list_probe_t;

static const dtrace_pattr_t	pattr = {
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_ISA },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_ISA },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
};

dt_provimpl_t	dt_pid;
dt_provimpl_t	dt_usdt;

static int populate(dtrace_hdl_t *dtp)
{
	dt_provider_create(dtp, dt_uprobe.name, &dt_uprobe, &pattr);
	dt_provider_create(dtp, dt_pid.name, &dt_pid, &pattr);
	dt_provider_create(dtp, dt_usdt.name, &dt_usdt, &pattr);

	return 0;
}

static void free_probe_list(dtrace_hdl_t *dtp, list_probe_t *elem)
{
	while (elem != NULL) {
		list_probe_t	*next;

		next = dt_list_next(elem);
		dt_free(dtp, elem);
		elem = next;
	}
}

/*
 * Destroy an underlying (uprobe) probe.
 */
static void probe_destroy_underlying(dtrace_hdl_t *dtp, void *datap)
{
	dt_uprobe_t	*upp = datap;
	tp_probe_t	*tpp = upp->tp;

	dt_tp_destroy(dtp, tpp);
	free_probe_list(dtp, dt_list_next(&upp->probes));
	dt_free(dtp, upp->fn);
	dt_free(dtp, upp);
}

/*
 * Destroy an overlying (pid/USDT) probe.
 */
static void probe_destroy(dtrace_hdl_t *dtp, void *datap)
{
	free_probe_list(dtp, datap);
}


/*
 * Look up or create an underlying (real) probe, corresponding directly to a
 * uprobe.  Since multiple pid and USDT probes may all map onto the same
 * underlying probe, we may already have one in the system.
 *
 * If not found, we create a new probe.
 */
static dt_probe_t *create_underlying(dtrace_hdl_t *dtp,
				     const pid_probespec_t *psp)
{
	char			mod[DTRACE_MODNAMELEN];
	char			prb[DTRACE_NAMELEN];
	dtrace_probedesc_t	pd;
	dt_probe_t		*prp;
	dt_uprobe_t		*upp;

	/*
	 * The underlying probes (uprobes) represent the tracepoints that pid
	 * and USDT probes are associated with.  They follow a standard naming
	 * convention because an underlying probe could be a tracepoint for one
	 * or more pid and/or USDT probes.
	 *
	 * The probe description for non-return probes is:
	 *
	 *	uprobe:<dev>_<inode>:<func>:<offset>
	 *
	 * The probe description for return probes is:
	 *
	 *	uprobe:<dev>_<inode>:<func>:return
	 */
	snprintf(mod, sizeof(mod), "%lx_%lx", psp->pps_dev, psp->pps_inum);

	switch (psp->pps_type) {
	case DTPPT_RETURN:
		strcpy(prb, "return");
		break;
	case DTPPT_ENTRY:
	case DTPPT_OFFSETS:
		snprintf(prb, sizeof(prb), "%lx", psp->pps_off);
		break;
	default:
		dt_dprintf("pid: unknown PID probe type %i\n", psp->pps_type);
		return NULL;
	}

	pd.id = DTRACE_IDNONE;
	pd.prv = prvname;
	pd.mod = mod;
	pd.fun = psp->pps_fun;
	pd.prb = prb;

	prp = dt_probe_lookup(dtp, &pd);
	if (prp == NULL) {
		dt_provider_t	*pvp;

		/* Get the provider for underlying probes. */
		pvp = dt_provider_lookup(dtp, prvname);
		if (pvp == NULL)
			return NULL;

		/* Set up the probe data. */
		upp = dt_zalloc(dtp, sizeof(dt_uprobe_t));
		if (upp == NULL)
			return NULL;

		upp->dev = psp->pps_dev;
		upp->inum = psp->pps_inum;
		upp->off = psp->pps_off;
		if (psp->pps_fn)
			upp->fn = strdup(psp->pps_fn);
		upp->tp = dt_tp_alloc(dtp);
		if (upp->tp == NULL)
			goto fail;

		prp = dt_probe_insert(dtp, pvp, pd.prv, pd.mod, pd.fun, pd.prb,
				      upp);
		if (prp == NULL)
			goto fail;
	} else
		upp = prp->prv_data;

	if (psp->pps_type == DTPPT_RETURN)
		upp->flags |= PP_IS_RETURN;

	return prp;

fail:
	probe_destroy(dtp, upp);
	return NULL;
}

static int provide_probe(dtrace_hdl_t *dtp, const pid_probespec_t *psp,
			 const char *prb, const dt_provimpl_t *pvops, int flags)
{
	char			prv[DTRACE_PROVNAMELEN];
	dt_provider_t		*pvp;
	dtrace_probedesc_t	pd;
	dt_uprobe_t		*upp;
	dt_probe_t		*prp, *uprp;
	list_probe_t		*pop, *pup;

	snprintf(prv, sizeof(prv), "%s%d", psp->pps_prv, psp->pps_pid);

	pd.id = DTRACE_IDNONE;
	pd.prv = prv;
	pd.mod = psp->pps_mod;
	pd.fun = psp->pps_fun;
	pd.prb = prb;

	/* Get (or create) the provider for the PID of the probe. */
	pvp = dt_provider_lookup(dtp, prv);
	if (pvp == NULL) {
		pvp = dt_provider_create(dtp, prv, pvops, &pattr);
		if (pvp == NULL)
			return -1;
	}

	/* Mark the provider as a PID-based provider. */
	pvp->pv_flags |= DT_PROVIDER_PID;

	uprp = create_underlying(dtp, psp);
	if (uprp == NULL)
		return -1;

	upp = uprp->prv_data;
	upp->flags |= flags;

	prp = dt_probe_lookup(dtp, &pd);
	if (prp != NULL) {
		/*
		 * Probe already exists.  If it's already in the underlying
		 * probe's probe list, there is nothing left to do.
		 */
		for (pop = dt_list_next(&upp->probes); pop != NULL;
		     pop = dt_list_next(pop)) {
			if (pop->probe == prp)
				return 0;
		}
	}

	/*
	 * Underlying and overlying probe list entries.
	 */
	pop = dt_zalloc(dtp, sizeof(list_probe_t));
	if (pop == NULL)
		return -1;

	pup = dt_zalloc(dtp, sizeof(list_probe_t));
	if (pup == NULL) {
		dt_free(dtp, pop);
		return -1;
	}

	/* Add the pid probe, if we need to. */

	pup->probe = uprp;
	if (prp == NULL)
		prp = dt_probe_insert(dtp, pvp, pd.prv, pd.mod, pd.fun, pd.prb,
				      pup);
	else
		dt_list_append((dt_list_t *)prp->prv_data, pup);

	if (prp == NULL) {
		dt_free(dtp, pop);
		dt_free(dtp, pup);
		return -1;
	}

	pop->probe = prp;

	/*
	 * Add the pid probe to the list of probes for the underlying probe.
	 */
	dt_list_append(&upp->probes, pop);

	return 0;
}

static int provide_pid_probe(dtrace_hdl_t *dtp, const pid_probespec_t *psp)
{
	char	prb[DTRACE_NAMELEN];

	switch (psp->pps_type) {
	case DTPPT_ENTRY:
		strcpy(prb, "entry");
		break;
	case DTPPT_RETURN:
		strcpy(prb, "return");
		break;
	case DTPPT_OFFSETS:
		snprintf(prb, sizeof(prb), "%lx", psp->pps_off);
		break;
	default:
		dt_dprintf("pid: unknown PID probe type %i\n", psp->pps_type);
		return -1;
	}

	return provide_probe(dtp, psp, prb, &dt_usdt, PP_IS_MINE);
}

static int provide_usdt_probe(dtrace_hdl_t *dtp, const pid_probespec_t *psp)
{
	if (psp->pps_type != DTPPT_OFFSETS) {
		dt_dprintf("pid: unknown USDT probe type %i\n", psp->pps_type);
		return -1;
	}

	return provide_probe(dtp, psp, psp->pps_prb, &dt_usdt, PP_IS_FUNCALL);
}

static void enable(dtrace_hdl_t *dtp, dt_probe_t *prp)
{
	const list_probe_t	*pup;

	assert(prp->prov->impl == &dt_pid || prp->prov->impl == &dt_usdt);

	/*
	 * We need to enable the underlying probes (if not enabled yet).
	 */
	for (pup = prp->prv_data; pup != NULL; pup = dt_list_next(pup)) {
		dt_probe_t *uprp = pup->probe;
		dt_probe_enable(dtp, uprp);
	}

	/*
	 * Finally, ensure we're in the list of enablings as well.
	 * (This ensures that, among other things, the probes map
	 * gains entries for us.)
	 */
	if (!dt_in_list(&dtp->dt_enablings, prp))
		dt_list_append(&dtp->dt_enablings, prp);
}

/*
 * Generate a BPF trampoline for a pid probe.
 *
 * The trampoline function is called when a pid probe triggers, and it must
 * satisfy the following prototype:
 *
 *	int dt_pid(dt_pt_regs *regs)
 *
 * The trampoline will first populate a dt_dctx_t struct.  It will then emulate
 * the firing of all dependent pid* probes and their clauses.
 */
static void trampoline(dt_pcb_t *pcb)
{
	dt_irlist_t		*dlp = &pcb->pcb_ir;
	const dt_probe_t	*prp = pcb->pcb_probe;
	const dt_uprobe_t	*upp = prp->prv_data;
	const list_probe_t	*pop;
	uint_t			lbl_exit = pcb->pcb_exitlbl;

	dt_cg_tramp_prologue(pcb);

	/*
	 * After the dt_cg_tramp_prologue() call, we have:
	 *				//     (%r7 = dctx->mst)
	 *				//     (%r8 = dctx->ctx)
	 */

	dt_cg_tramp_copy_regs(pcb);
	if (upp->flags & PP_IS_RETURN)
		dt_cg_tramp_copy_rval_from_regs(pcb);
	else
		dt_cg_tramp_copy_args_from_regs(pcb,
						!(upp->flags & PP_IS_FUNCALL));

	/*
	 * Retrieve the PID of the process that caused the probe to fire.
	 */
	emit(dlp,  BPF_CALL_HELPER(BPF_FUNC_get_current_pid_tgid));
	emit(dlp,  BPF_ALU64_IMM(BPF_RSH, BPF_REG_0, 32));

	/*
	 * Generate a composite conditional clause:
	 *
	 *	if (pid == PID1) {
	 *		dctx->mst->prid = PRID1;
	 *		< any number of clause calls >
	 *		goto exit;
	 *	} else if (pid == PID2) {
	 *		dctx->mst->prid = PRID2;
	 *		< any number of clause calls >
	 *		goto exit;
	 *	} else if (pid == ...) {
	 *		< ... >
	 *	}
	 *
	 * It is valid and safe to use %r0 to hold the pid value because there
	 * are no assignments to %r0 possible in between the conditional
	 * statements.
	 */
	for (pop = dt_list_next(&upp->probes); pop != NULL;
	     pop = dt_list_next(pop)) {
		const dt_probe_t	*pprp = pop->probe;
		uint_t			lbl_next = dt_irlist_label(dlp);
		pid_t			pid;
		dt_ident_t		*idp;

		pid = dt_pid_get_pid(pprp->desc, pcb->pcb_hdl, pcb, NULL);
		assert(pid != -1);

		idp = dt_dlib_add_probe_var(pcb->pcb_hdl, pprp);
		assert(idp != NULL);

		/*
		 * Check whether this pid-provider probe serves the current
		 * process, and emit a sequence of clauses for it when it does.
		 */
		emit(dlp,  BPF_BRANCH_IMM(BPF_JNE, BPF_REG_0, pid, lbl_next));
		emite(dlp, BPF_STORE_IMM(BPF_W, BPF_REG_7, DMST_PRID, pprp->desc->id), idp);
		dt_cg_tramp_call_clauses(pcb, pprp, DT_ACTIVITY_ACTIVE);
		emit(dlp,  BPF_JUMP(lbl_exit));
		emitl(dlp, lbl_next,
			   BPF_NOP());
	}

	dt_cg_tramp_return(pcb);
}

static int attach(dtrace_hdl_t *dtp, const dt_probe_t *prp, int bpf_fd)
{
	dt_uprobe_t	*upp = prp->prv_data;
	tp_probe_t	*tpp = upp->tp;
	FILE		*f;
	char		*fn;
	char		*prb = NULL;
	int		rc = -1;

	if (dt_tp_is_created(tpp))
		goto attach_bpf;

	if (upp->flags & PP_IS_MINE) {
		char	*spec;

		assert(upp->fn != NULL);

		if (asprintf(&spec, "%s:0x%lx", upp->fn, upp->off) < 0)
			return -ENOENT;

		prb = uprobe_create(upp->dev, upp->inum, upp->off, spec,
				    upp->flags & PP_IS_RETURN);
		free(spec);

		/*
		 * If the uprobe creation failed, it is possible it already
		 * existed because someone else created it.  Tey to access its
		 * tracefs info and if that fail, we really failed.
		 */
	}

	if (prb == NULL)
		prb = uprobe_name(upp->dev, upp->inum, upp->off,
				  upp->flags & PP_IS_RETURN);

	/* open format file */
	rc = asprintf(&fn, "%s%s/format", EVENTSFS, prb);
	free(prb);
	if (rc < 0)
		return -ENOENT;
	f = fopen(fn, "r");
	free(fn);
	if (f == NULL)
		return -ENOENT;

	rc = dt_tp_event_info(dtp, f, 0, tpp, NULL, NULL);
	fclose(f);

	if (rc < 0)
		return -ENOENT;

attach_bpf:
	/* attach BPF program to the probe */
	return dt_tp_attach(dtp, tpp, bpf_fd);
}

static int probe_info(dtrace_hdl_t *dtp, const dt_probe_t *prp,
		      int *argcp, dt_argdesc_t **argvp)
{
	*argcp = 0;			/* no known arguments */
	*argvp = NULL;

	return 0;
}

/*
 * Try to clean up system resources that may have been allocated for this
 * probe.
 *
 * If there is an event FD, we close it.
 *
 * We also try to remove any uprobe that may have been created for the probe
 * (but only if we created it, not if dtprobed did).  This is harmless for
 * probes that didn't get created.  If the removal fails for some reason we are
 * out of luck - fortunately it is not harmful to the system as a whole.
 */
static void detach(dtrace_hdl_t *dtp, const dt_probe_t *prp)
{
	dt_uprobe_t	*upp = prp->prv_data;
	tp_probe_t	*tpp = upp->tp;

	if (!dt_tp_is_created(tpp))
		return;

	dt_tp_detach(dtp, tpp);

	if (!(upp->flags & PP_IS_MINE))
		return;

	uprobe_delete(upp->dev, upp->inum, upp->off, upp->flags & PP_IS_RETURN);
}

/*
 * Used for underlying probes (uprobes).
 */
dt_provimpl_t	dt_uprobe = {
	.name		= prvname,
	.prog_type	= BPF_PROG_TYPE_KPROBE,
	.populate	= &populate,
	.trampoline	= &trampoline,
	.attach		= &attach,
	.probe_info	= &probe_info,
	.detach		= &detach,
	.probe_destroy	= &probe_destroy_underlying,
};

/*
 * Used for pid probes.
 */
dt_provimpl_t	dt_pid = {
	.name		= "pid",
	.prog_type	= BPF_PROG_TYPE_UNSPEC,
	.provide_probe	= &provide_pid_probe,
	.enable		= &enable,
	.probe_destroy	= &probe_destroy,
};

/*
 * Used for usdt probes.
 */
dt_provimpl_t	dt_usdt = {
	.name		= "usdt",
	.prog_type	= BPF_PROG_TYPE_UNSPEC,
	.provide_probe	= &provide_usdt_probe,
	.enable		= &enable,
	.probe_destroy	= &probe_destroy,
};
