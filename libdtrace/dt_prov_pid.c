/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * The PID provider for DTrace.
 */
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

static const char		prvname[] = "pid";

#define UPROBE_EVENTS		TRACEFS "uprobe_events"

#define PID_GROUP_FMT		GROUP_FMT "_%lx"
#define PID_GROUP_DATA		GROUP_DATA, pp->ino

typedef struct pid_probe {
	ino_t		ino;
	char		*fn;
	uint64_t	off;
	tp_probe_t	*tp;
	dt_list_t	probes;
} pid_probe_t;

static const dtrace_pattr_t	pattr = {
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_ISA },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_ISA },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
};

dt_provimpl_t	dt_pid_proc;

static int populate(dtrace_hdl_t *dtp)
{
	dt_provider_create(dtp, prvname, &dt_pid, &pattr);
	return 0;
}

static void probe_destroy(dtrace_hdl_t *dtp, void *datap)
{
	pid_probe_t	*pp = datap;
	tp_probe_t	*tpp = pp->tp;

	dt_tp_destroy(dtp, tpp);
	dt_free(dtp, pp->fn);
	dt_free(dtp, pp);
}

static int provide_pid(dtrace_hdl_t *dtp, const pid_probespec_t *psp)
{
	char			prv[DTRACE_PROVNAMELEN];
	char			mod[DTRACE_MODNAMELEN];
	char			prb[DTRACE_NAMELEN];
	dt_provider_t		*pidpvp;
	dt_provider_t		*pvp;
	dtrace_probedesc_t	pd;
	pid_probe_t		*pp;
	dt_probe_t		*prp;
	uint64_t		off = psp->pps_pc - psp->pps_vaddr;

	/*
	 * First check whether this pid probe already exists.  If so, there is
	 * nothing left to do.
	 */
	snprintf(prv, sizeof(prv), PID_PRVNAME, psp->pps_pid);

	switch (psp->pps_type) {
	case DTPPT_ENTRY:
		strncpy(prb, "entry", sizeof(prb));
		break;
	case DTPPT_RETURN:
		strncpy(prb, "return", sizeof(prb));
		break;
	case DTPPT_OFFSETS:
		snprintf(prb, sizeof(prb), "%lx", off);
		break;
	default:
		return 0;
	}

	pd.id = DTRACE_IDNONE;
	pd.prv = prv;
	pd.mod = psp->pps_mod;
	pd.fun = psp->pps_fun;
	pd.prb = prb;

	prp = dt_probe_lookup(dtp, &pd);
	if (prp != NULL)
		return 1;		/* probe found */

	/* Get the main (real) pid provider. */
	pidpvp = dt_provider_lookup(dtp, prvname);
	if (pidpvp == NULL)
		return 0;

	/* Get (or create) the provider for the PID of the probe. */
	pvp = dt_provider_lookup(dtp, prv);
	if (pvp == NULL) {
		pvp = dt_provider_create(dtp, prv, &dt_pid_proc, &pattr);
		if (pvp == NULL)
			return 0;
	}

	/* Mark the provider as a PID provider. */
	pvp->pv_flags |= DT_PROVIDER_PID;

	/*
	 * Fill in the probe description for the main (real) probe.  The
	 * module is the inode number (in hex), the function name is as
	 * specified for the pid probe, and the probe name is "entry",
	 * "return", or the offset into the function (in hex).
	 */
	snprintf(mod, sizeof(mod), "%lx", psp->pps_ino);

	/*
	 * Try to lookup the main (real) probe.  Since multiple pid probes may
	 * all map onto the same underlying main (real) probe, we may already
	 * have one in the system.
	 *
	 * If not found, we create a new probe.
	 */
	pd.id = DTRACE_IDNONE;
	pd.prv = prvname;
	pd.mod = mod;
	pd.fun = psp->pps_fun;
	pd.prb = prb;
	prp = dt_probe_lookup(dtp, &pd);
	if (prp == NULL) {
		/* Set up the pid probe data. */
		pp = dt_zalloc(dtp, sizeof(pid_probe_t));
		if (pp == NULL)
			return 0;

		pp->ino = psp->pps_ino;
		pp->fn = strdup(psp->pps_fn);
		pp->off = off;
		pp->tp = dt_tp_alloc(dtp);
		if (pp->tp == NULL)
			goto fail;

		prp = dt_probe_insert(dtp, pidpvp, prvname, mod, psp->pps_fun,
				      prb, pp);
		if (prp == NULL)
			goto fail;
	} else
		pp = prp->prv_data;

	/* Try to add the pid probe. */
	prp = dt_probe_insert(dtp, pvp, prv, psp->pps_mod, psp->pps_fun, prb,
			      prp);
	if (prp == NULL)
		goto fail;

	/* Add the pid probe to the list of probes for the main (real) probe. */
	dt_list_append(&pp->probes, prp);

	return 1;

fail:
	probe_destroy(dtp, pp);
	return 0;
}

static void enable(dtrace_hdl_t *dtp, dt_probe_t *prp)
{
	assert(prp->prov->impl == &dt_pid_proc);

	/* We need to enable the main (real) probe (if not enabled yet). */
	dt_probe_enable(dtp, (dt_probe_t *)prp->prv_data);
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
	const dt_probe_t	*pprp;
	const pid_probe_t	*pp = prp->prv_data;
	uint_t			lbl_exit = pcb->pcb_exitlbl;

	dt_cg_tramp_prologue(pcb);

	/*
	 * After the dt_cg_tramp_prologue() call, we have:
	 *				//     (%r7 = dctx->mst)
	 *				//     (%r8 = dctx->ctx)
	 */

	dt_cg_tramp_copy_regs(pcb, BPF_REG_8);
	if (strcmp(pcb->pcb_probe->desc->prb, "return") == 0)
		dt_cg_tramp_copy_rval_from_regs(pcb, BPF_REG_8);
	else
		dt_cg_tramp_copy_args_from_regs(pcb, BPF_REG_8);

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
	for (pprp = dt_list_next(&pp->probes); pprp != NULL;
	     pprp = dt_list_next(pprp)) {
		uint_t		lbl_next = dt_irlist_label(dlp);
		pid_t		pid = strtoul(pprp->desc->prv + 3, NULL, 10);
		char		pn[DTRACE_FULLNAMELEN + 1];
		dt_ident_t	*idp;

		snprintf(pn, DTRACE_FULLNAMELEN, "%s:%s:%s:%s",
			 pprp->desc->prv, pprp->desc->mod, pprp->desc->fun,
			 pprp->desc->prb);
		idp = dt_dlib_add_var(pcb->pcb_hdl, pn, pprp->desc->id);
		assert(idp != NULL);
		/*
		 * Check whether this pid-provider probe serves the current
		 * process.  This loop creates a sequence
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
	pid_probe_t	*pp = prp->prv_data;
	tp_probe_t	*tpp = pp->tp;

	if (!dt_tp_is_created(tpp)) {
		char	*fn;
		FILE	*f;
		size_t	len;
		int	fd, rc = -1;

		/* add the uprobe */
		fd = open(UPROBE_EVENTS, O_WRONLY | O_APPEND);
		if (fd != -1) {
			rc = dprintf(fd,
				     "%c:" PID_GROUP_FMT "/%s_%s %s:0x%lx\n",
				     prp->desc->prb[0] == 'e' ? 'p' : 'r',
				     PID_GROUP_DATA, prp->desc->fun,
				     prp->desc->prb, pp->fn, pp->off);
			close(fd);
		}
		if (rc == -1)
			return -ENOENT;

		/* open format file */
		len = snprintf(NULL, 0, "%s" PID_GROUP_FMT "/%s_%s/format",
			       EVENTSFS, PID_GROUP_DATA, prp->desc->fun,
			       prp->desc->prb) + 1;
		fn = dt_alloc(dtp, len);
		if (fn == NULL)
			return -ENOENT;

		snprintf(fn, len, "%s" PID_GROUP_FMT "/%s_%s/format",
			 EVENTSFS, PID_GROUP_DATA, prp->desc->fun,
			 prp->desc->prb);
		f = fopen(fn, "r");
		dt_free(dtp, fn);
		if (f == NULL)
			return -ENOENT;

		rc = dt_tp_event_info(dtp, f, 0, tpp, NULL, NULL);
		fclose(f);

		if (rc < 0)
			return -ENOENT;
	}

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
 * We also try to remove any uprobe that may have been created for the probe.
 * This is harmless for probes that didn't get created.  If the removal fails
 * for some reason we are out of luck - fortunately it is not harmful to the
 * system as a whole.
 */
static void detach(dtrace_hdl_t *dtp, const dt_probe_t *prp)
{
	int		fd;
	pid_probe_t	*pp = prp->prv_data;
	tp_probe_t	*tpp = pp->tp;

	if (!dt_tp_is_created(tpp))
		return;

	dt_tp_detach(dtp, tpp);

	fd = open(UPROBE_EVENTS, O_WRONLY | O_APPEND);
	if (fd == -1)
		return;

	dprintf(fd, "-:" PID_GROUP_FMT "/%s_%s\n", PID_GROUP_DATA,
		prp->desc->fun, prp->desc->prb);
	close(fd);
}

dt_provimpl_t	dt_pid = {
	.name		= prvname,
	.prog_type	= BPF_PROG_TYPE_KPROBE,
	.populate	= &populate,
	.provide_pid	= &provide_pid,
	.trampoline	= &trampoline,
	.attach		= &attach,
	.probe_info	= &probe_info,
	.detach		= &detach,
	.probe_destroy	= &probe_destroy,
};

dt_provimpl_t	dt_pid_proc = {
	.name		= prvname,
	.prog_type	= BPF_PROG_TYPE_KPROBE,
	.provide_pid	= &provide_pid,
	.enable		= &enable,
	.trampoline	= &trampoline,
};
