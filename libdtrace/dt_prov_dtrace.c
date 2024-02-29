/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * The core probe provider for DTrace for the BEGIN, END, and ERROR probes.
 */
#include <assert.h>
#include <errno.h>
#include <string.h>

#include <bpf_asm.h>

#include "dt_dctx.h"
#include "dt_cg.h"
#include "dt_provider_tp.h"
#include "dt_probe.h"

static const char		prvname[] = "dtrace";
static const char		modname[] = "";
static const char		funname[] = "";

#define PROBE_FUNC_SUFFIX	"_probe"

#define UPROBE_EVENTS		TRACEFS "uprobe_events"

static const dtrace_pattr_t	pattr = {
{ DTRACE_STABILITY_STABLE, DTRACE_STABILITY_STABLE, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_STABLE, DTRACE_STABILITY_STABLE, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_STABLE, DTRACE_STABILITY_STABLE, DTRACE_CLASS_COMMON },
};

static int populate(dtrace_hdl_t *dtp)
{
	dt_provider_t	*prv;
	dt_probe_t	*prp;
	int		n = 0;

	prv = dt_provider_create(dtp, prvname, &dt_dtrace, &pattr, NULL);
	if (prv == NULL)
		return -1;			/* errno already set */

	prp = dt_tp_probe_insert(dtp, prv, prvname, modname, funname, "BEGIN");
	if (prp) {
		n++;
		dt_probe_enable(dtp, prp);
	}

	prp = dt_tp_probe_insert(dtp, prv, prvname, modname, funname, "END");
	if (prp) {
		n++;
		dt_probe_enable(dtp, prp);
	}

	prp = dt_tp_probe_insert(dtp, prv, prvname, modname, funname, "ERROR");
	if (prp) {
		n++;
		dt_probe_enable(dtp, prp);
		dtp->dt_error = prp;
	}

	return n;
}

/*
 * Generate a BPF trampoline for a dtrace probe (BEGIN, END, or ERROR).
 *
 * The trampoline function is called when a dtrace probe triggers, and it must
 * satisfy the following prototype:
 *
 *	int dt_dtrace(dt_pt_regs *regs)
 *
 * The trampoline will populate a dt_dctx_t struct and then call the function
 * that implements the compiled D clause.  It returns 0 to the caller.
 */
static int trampoline(dt_pcb_t *pcb, uint_t exitlbl)
{
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	dt_activity_t	act = DT_ACTIVITY_ACTIVE;
	uint32_t	i, key = 0;

	/*
	 * The ERROR probe isn't really a trace event that a BPF program is
	 * attached to.  Its entire trampoline program is provided by the code
	 * generator.
	 */
	if (strcmp(pcb->pcb_probe->desc->prb, "ERROR") == 0) {
		dt_cg_tramp_error(pcb);
		return 0;
	}

	/*
	 * The BEGIN probe should only run when the activity state is INACTIVE,
	 * for this process's PID (TGID).
	 * At the end of the trampoline (after executing any clauses), the
	 * state must be advanced to the next state (INACTIVE -> ACTIVE, or if
	 * there was an exit() action in the clause, DRAINING -> STOPPED).
	 *
	 * When the BEGIN probe is triggered, we need to record the CPU it runs
	 * on in state[DT_STATE_BEGANON] to ensure that we know which trace
	 * data buffer to process first.
	 *
	 * The END probe should only run when the activity state is DRAINING,
	 * for this process's PID (TGID).
	 * At the end of the trampoline (after executing any clauses), the
	 * state must be advanced to the next state (DRAINING -> STOPPED).
	 *
	 * When the END probe is triggered, we need to record the CPU it runs
	 * on in state[DT_STATE_ENDEDON] to ensure that we know which trace
	 * data buffer to process last.
	 */
	if (strcmp(pcb->pcb_probe->desc->prb, "BEGIN") == 0) {
		act = DT_ACTIVITY_INACTIVE;
		key = DT_STATE_BEGANON;
	} else if (strcmp(pcb->pcb_probe->desc->prb, "END") == 0) {
		act = DT_ACTIVITY_DRAINING;
		key = DT_STATE_ENDEDON;
	}

	/*
	 * Retrieve the PID (TGID) of the process that caused the probe to fire,
	 * and check it against the PID we're meant to be using.  Do it before
	 * the trampoline to minimize the cost of pointless firings in other
	 * tracers, even though this means preserving the context in %r1 around
	 * the call.
	 */
	emit(dlp, BPF_MOV_REG(BPF_REG_6, BPF_REG_1));
	emit(dlp, BPF_CALL_HELPER(BPF_FUNC_get_current_pid_tgid));
	emit(dlp, BPF_ALU64_IMM(BPF_RSH, BPF_REG_0, 32));
	emit(dlp, BPF_BRANCH_IMM(BPF_JNE, BPF_REG_0, getpid(), pcb->pcb_exitlbl));
	emit(dlp, BPF_MOV_REG(BPF_REG_1, BPF_REG_6));

	dt_cg_tramp_prologue_act(pcb, act);

	/*
	 * After the dt_cg_tramp_prologue_act() call, we have:
	 *				//     (%r7 = dctx->mst)
	 *				//     (%r8 = dctx->ctx)
	 */

	/*
	 *     key = DT_STATE_(BEGANON|ENDEDON);
	 *				// stw [%fp + DT_TRAMP_SP_SLOT(0)],
	 *				//	DT_STATE_(BEGANON|ENDEDON)
	 *     val = bpf_get_smp_processor_id();
	 *				// call bpf_get_smp_processor_id
	 *				// stw [%fp + DT_TRAMP_SP_SLOT(1)], %r0
	 *     bpf_map_update_elem(state, &key, &val, BPF_ANY);
	 *				// lddw %r1, &state
	 *				// mov %r2, %fp
	 *				// add %r2, DT_TRAMP_SP_SLOT(0)
	 *				// mov %r3, %fp
	 *				// add %r3, DT_TRAMP_SP_SLOT(1)
	 *				// mov %r4, BPF_ANY
	 *				// call bpf_map_update_elem
	 */
	dt_ident_t	*state = dt_dlib_get_map(pcb->pcb_hdl, "state");

	emit(dlp, BPF_STORE_IMM(BPF_W, BPF_REG_FP, DT_TRAMP_SP_SLOT(0), key));
	emit(dlp, BPF_CALL_HELPER(BPF_FUNC_get_smp_processor_id));
	emit(dlp, BPF_STORE(BPF_W, BPF_REG_FP, DT_TRAMP_SP_SLOT(1), BPF_REG_0));
	dt_cg_xsetx(dlp, state, DT_LBL_NONE, BPF_REG_1, state->di_id);
	emit(dlp, BPF_MOV_REG(BPF_REG_2, BPF_REG_FP));
	emit(dlp, BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, DT_TRAMP_SP_SLOT(0)));
	emit(dlp, BPF_MOV_REG(BPF_REG_3, BPF_REG_FP));
	emit(dlp, BPF_ALU64_IMM(BPF_ADD, BPF_REG_3, DT_TRAMP_SP_SLOT(1)));
	emit(dlp, BPF_MOV_IMM(BPF_REG_4, BPF_ANY));
	emit(dlp, BPF_CALL_HELPER(BPF_FUNC_map_update_elem));

	dt_cg_tramp_copy_regs(pcb);

	/* zero the probe args */
	for (i = 0; i < ARRAY_SIZE(((dt_mstate_t *)0)->argv); i++)
		emit(dlp, BPF_STORE_IMM(BPF_DW, BPF_REG_7, DMST_ARG(i), 0));

	dt_cg_tramp_epilogue_advance(pcb, act);

	return 0;
}

static char *uprobe_spec(pid_t pid, const char *prb)
{
	struct ps_prochandle	*P;
	int			perr = 0;
	char			*fun;
	GElf_Sym		sym;
	prsyminfo_t		si;
	char			*spec = NULL;

	if (asprintf(&fun, "%s%s", prb, PROBE_FUNC_SUFFIX) < 0)
		return NULL;

	/* grab the process */
	P = Pgrab(pid, 2, 0, NULL, &perr);
	if (P == NULL) {
		free(fun);
		return NULL;
	}

	/* look up function, get the map, and record */
	if (Pxlookup_by_name(P, -1, PR_OBJ_EVERY, fun, &sym, &si) == 0) {
		const prmap_t	*mapp;

		mapp = Paddr_to_map(P, sym.st_value);
		if (mapp == NULL)
			goto out;

		if (mapp->pr_file->first_segment != mapp)
			mapp = mapp->pr_file->first_segment;

		asprintf(&spec, "%s:0x%lx", mapp->pr_file->prf_mapname,
			 sym.st_value - mapp->pr_vaddr);
	}

out:
	free(fun);
	Prelease(P, PS_RELEASE_NORMAL);
	Pfree(P);

	return spec;
}

static int attach(dtrace_hdl_t *dtp, const dt_probe_t *prp, int bpf_fd)
{
	tp_probe_t	*tpp = prp->prv_data;

	if (!dt_tp_is_created(tpp)) {
		char	*spec;
		char	*fn;
		FILE	*f;
		size_t	len;
		int	fd, rc = -1;

		/* get a uprobe specification for this probe */
		spec = uprobe_spec(getpid(), prp->desc->prb);
		if (spec == NULL)
			return -ENOENT;

		/* add a uprobe */
		fd = open(UPROBE_EVENTS, O_WRONLY | O_APPEND);
		if (fd != -1) {
			rc = dprintf(fd, "p:" GROUP_FMT "/%s %s\n",
				     GROUP_DATA, prp->desc->prb, spec);
			close(fd);
		}
		free(spec);
		if (rc == -1)
			return -ENOENT;

		/* open format file */
		len = snprintf(NULL, 0, "%s" GROUP_FMT "/%s/format",
			       EVENTSFS, GROUP_DATA, prp->desc->prb) + 1;
		fn = dt_alloc(dtp, len);
		if (fn == NULL)
			return -ENOENT;

		snprintf(fn, len, "%s" GROUP_FMT "/%s/format",
			 EVENTSFS, GROUP_DATA, prp->desc->prb);
		f = fopen(fn, "r");
		dt_free(dtp, fn);
		if (f == NULL)
			return -ENOENT;

		rc = dt_tp_event_info(dtp, f, 0, tpp, NULL, NULL);
		fclose(f);

		if (rc < 0)
			return -ENOENT;
	}

	/* attach BPF program to the tracepoint */
	return dt_tp_attach(dtp, tpp, bpf_fd);
}

static int probe_info(dtrace_hdl_t *dtp, const dt_probe_t *prp,
		      int *argcp, dt_argdesc_t **argvp)
{
	*argcp = 0;			/* no arguments */
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
	tp_probe_t	*tpp = prp->prv_data;
	int		fd;

	if (!dt_tp_is_created(tpp))
		return;

	dt_tp_detach(dtp, tpp);

	fd = open(UPROBE_EVENTS, O_WRONLY | O_APPEND);
	if (fd == -1)
		return;

	dprintf(fd, "-:" GROUP_FMT "/%s\n", GROUP_DATA, prp->desc->prb);
	close(fd);
}

dt_provimpl_t	dt_dtrace = {
	.name		= prvname,
	.prog_type	= BPF_PROG_TYPE_KPROBE,
	.populate	= &populate,
	.trampoline	= &trampoline,
	.attach		= &attach,
	.probe_info	= &probe_info,
	.detach		= &detach,
	.probe_destroy	= &dt_tp_probe_destroy,
};
