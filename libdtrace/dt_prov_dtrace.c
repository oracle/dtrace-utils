/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * The core probe provider for DTrace for the BEGIN, END, and ERROR probes.
 */
#include <assert.h>
#include <errno.h>
#include <string.h>

#include <bpf_asm.h>

#include "dt_provider.h"
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

	prv = dt_provider_create(dtp, prvname, &dt_dtrace, &pattr);
	if (prv == NULL)
		return 0;

	prp = tp_probe_insert(dtp, prv, prvname, modname, funname, "BEGIN");
	if (prp) {
		n++;
		dt_list_append(&dtp->dt_enablings, prp);
	}
	prp = tp_probe_insert(dtp, prv, prvname, modname, funname, "END");
	if (prp) {
		n++;
		dt_list_append(&dtp->dt_enablings, prp);
	}
	if (tp_probe_insert(dtp, prv, prvname, modname, funname, "ERROR"))
		n++;

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
static void trampoline(dt_pcb_t *pcb)
{
	int		i;
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	struct bpf_insn	instr;
	uint_t		lbl_exit = dt_irlist_label(dlp);
	dt_activity_t	act;
	int		adv_act;
	uint32_t	key = 0;

	/*
	 * The BEGIN probe should only run when the activity state is INACTIVE.
	 * At the end of the trampoline (after executing any clauses), the
	 * state must be advanced to the next state (INACTIVE -> ACTIVE).
	 *
	 * When the BEGIN probe is triggered, we need to record the CPU it runs
	 * on in state[DT_STATE_BEGANON] to ensure that we know which trace
	 * data buffer to process first.
	 *
	 * The END probe should only run when the activity state is ACTIVE.
	 * At the end of the trampoline (after executing any clauses), the
	 * state must be advanced to the next state (ACTIVE -> DRAINING).
	 *
	 * When the END probe is triggered, we need to record the CPU it runs
	 * on in state[DT_STATE_ENDEDON] to ensure that we know which trace
	 * data buffer to process last.
	 *
	 * Any other probe requires the state to be ACTIVE, and does not change
	 * the state.
	 */
	if (strcmp(pcb->pcb_probe->desc->prb, "BEGIN") == 0) {
		act = DT_ACTIVITY_INACTIVE;
		adv_act = 1;
		key = DT_STATE_BEGANON;
	} else if (strcmp(pcb->pcb_probe->desc->prb, "END") == 0) {
		act = DT_ACTIVITY_ACTIVE;
		adv_act = 1;
		key = DT_STATE_ENDEDON;
	} else {
		act = DT_ACTIVITY_ACTIVE;
		adv_act = 0;
	}

	dt_cg_tramp_prologue_act(pcb, lbl_exit, act);

	if (key) {
		/*
		 *     key = DT_STATE_(BEGANON|ENDEDON);
		 *			// stw [%fp + DT_STK_SPILL(0)],
		 *			//	DT_STATE_(BEGANON|ENDEDON)
		 *     val = bpf_get_smp_processor_id();
		 *			// call bpf_get_smp_processor_id
		 *			// stw [%fp + DT_STK_SPILL(1)], %r0
		 *     bpf_map_update_elem(state, &key, &val, BPF_ANY);
		 *			// lddw %r1, &state
		 *			// mov %r2, %fp
		 *			// add %r2, DT_STK_SPILL(0)
		 *			// mov %r3, %fp
		 *			// add %r3, DT_STK_SPILL(1)
		 *			// mov %r4, BPF_ANY
		 *			// call bpf_map_update_elem
		 */
		dt_ident_t	*state = dt_dlib_get_map(pcb->pcb_hdl, "state");

		instr = BPF_STORE_IMM(BPF_W, BPF_REG_FP, DT_STK_SPILL(0), key);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		instr = BPF_CALL_HELPER(BPF_FUNC_get_smp_processor_id);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		instr = BPF_STORE(BPF_W, BPF_REG_FP, DT_STK_SPILL(1),
				  BPF_REG_0);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		dt_cg_xsetx(dlp, state, DT_LBL_NONE, BPF_REG_1, state->di_id);
		instr = BPF_MOV_REG(BPF_REG_2, BPF_REG_FP);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		instr = BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, DT_STK_SPILL(0));
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		instr = BPF_MOV_REG(BPF_REG_3, BPF_REG_FP);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		instr = BPF_ALU64_IMM(BPF_ADD, BPF_REG_3, DT_STK_SPILL(1));
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		instr = BPF_MOV_IMM(BPF_REG_4, BPF_ANY);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		instr = BPF_CALL_HELPER(BPF_FUNC_map_update_elem);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	}

	/*
	 * We cannot assume anything about the state of any registers so set up
	 * the ones we need:
	 *				//     (%r7 = dctx->mst)
	 *				// lddw %r7, [%fp + DCTX_FP(DCTX_MST)]
	 *				//     (%r8 = dctx->ctx)
	 *				// lddw %r8, [%fp + DCTX_FP(DCTX_CTX)]
	 */
	instr = BPF_LOAD(BPF_DW, BPF_REG_7, BPF_REG_FP, DCTX_FP(DCTX_MST));
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_LOAD(BPF_DW, BPF_REG_8, BPF_REG_FP, DCTX_FP(DCTX_CTX));
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

#if 0
	/*
	 *     dctx->mst->regs = *(dt_pt_regs *)dctx->ctx;
	 *				// lddw %r0, [%r8 + 0]
	 *				// stdw [%r7 + DMST_REGS + 0], %r0
	 *				// lddw %r0, [%r8 + 8]
	 *				// stdw [%r7 + DMST_REGS + 8], %r0
	 *				//     (...)
	 */
	for (i = 0; i < sizeof(dt_pt_regs); i += 8) {
		instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_8, i);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		instr = BPF_STORE(BPF_DW, BPF_REG_7, DMST_REGS + i, BPF_REG_0);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	}
#endif

	/*
	 *	dctx->mst->argv[0] = PT_REGS_PARAM1((dt_pt_regs *)dctx->ctx);
	 *				// lddw %r0, [%r8 + PT_REGS_ARG0]
	 *				// stdw [%r7 + DMST_ARG(0)], %r0
	 *	dctx->mst->argv[1] = PT_REGS_PARAM2((dt_pt_regs *)dctx->ctx);
	 *				// lddw %r0, [%r8 + PT_REGS_ARG1]
	 *				// stdw [%r7 + DMST_ARG(1)], %r0
	 *	dctx->mst->argv[2] = PT_REGS_PARAM3((dt_pt_regs *)dctx->ctx);
	 *				// lddw %r0, [%r8 + PT_REGS_ARG2]
	 *				// stdw [%r7 + DMST_ARG(2)], %r0
	 *	dctx->mst->argv[3] = PT_REGS_PARAM4((dt_pt_regs *)dctx->ctx);
	 *				// lddw %r0, [%r8 + PT_REGS_ARG3]
	 *				// stdw [%r7 + DMST_ARG(3)], %r0
	 *	dctx->mst->argv[4] = PT_REGS_PARAM5((dt_pt_regs *)dctx->ctx);
	 *				// lddw %r0, [%r8 + PT_REGS_ARG4]
	 *				// stdw [%r7 + DMST_ARG(4)], %r0
	 *	dctx->mst->argv[5] = PT_REGS_PARAM6((dt_pt_regs *)dctx->ctx);
	 *				// lddw %r0, [%r8 + PT_REGS_ARG5]
	 *				// stdw [%r7 + DMST_ARG(5)], %r0
	 */
	instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_8, PT_REGS_ARG0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(0), BPF_REG_0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_8, PT_REGS_ARG1);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(1), BPF_REG_0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_8, PT_REGS_ARG2);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(2), BPF_REG_0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_8, PT_REGS_ARG3);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(3), BPF_REG_0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_8, PT_REGS_ARG4);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(4), BPF_REG_0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_8, PT_REGS_ARG5);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(5), BPF_REG_0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

	/*
	 *     (we clear dctx->mst->argv[6] and on)
	 */
	for (i = 6; i < ARRAY_SIZE(((dt_mstate_t *)0)->argv); i++) {
		instr = BPF_STORE_IMM(BPF_DW, BPF_REG_7, DCTX_FP(DMST_ARG(i)),
				      0);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	}

	if (adv_act)
		dt_cg_tramp_epilogue_advance(pcb, lbl_exit);
	else
		dt_cg_tramp_epilogue(pcb, lbl_exit);
}

static char *uprobe_spec(dtrace_hdl_t *dtp, const char *prb)
{
	struct ps_prochandle	*P;
	int			perr = 0;
	char			*fun;
	GElf_Sym		sym;
	prsyminfo_t		si;
	char			*spec = NULL;

	fun = dt_alloc(dtp, strlen(prb) + strlen(PROBE_FUNC_SUFFIX) + 1);
	if (fun == NULL)
		return NULL;

	strcpy(fun, prb);
	strcat(fun, PROBE_FUNC_SUFFIX);

	/* grab our process */
	P = Pgrab(getpid(), 2, 0, NULL, &perr);
	if (P == NULL) {
		dt_free(dtp, fun);
		return NULL;
	}

	/* look up function, get the map, and record */
	if (Pxlookup_by_name(P, -1, PR_OBJ_EVERY, fun, &sym, &si) == 0) {
		const prmap_t	*mapp;
		size_t		len;

		mapp = Paddr_to_map(P, sym.st_value);
		if (mapp == NULL)
			goto out;

		if (mapp->pr_file->first_segment != mapp)
			mapp = mapp->pr_file->first_segment;

		len = snprintf(NULL, 0, "%s:0x%lx",
			       mapp->pr_file->prf_mapname,
			       sym.st_value - mapp->pr_vaddr) + 1;
		spec = dt_alloc(dtp, len);
		if (spec == NULL)
			goto out;

		snprintf(spec, len, "%s:0x%lx", mapp->pr_file->prf_mapname,
			 sym.st_value - mapp->pr_vaddr);
	}

out:
	dt_free(dtp, fun);
	Prelease(P, PS_RELEASE_NORMAL);
	Pfree(P);

	return spec;
}

static int attach(dtrace_hdl_t *dtp, const dt_probe_t *prp, int bpf_fd)
{
	tp_probe_t	*datap = prp->prv_data;

	if (datap->event_id == -1) {
		char	*spec;
		char	*fn;
		FILE	*f;
		size_t	len;
		int	fd, rc = -1;

		/* get a uprobe specification for this probe */
		spec = uprobe_spec(dtp, prp->desc->prb);
		if (spec == NULL)
			return -ENOENT;

		/* add a uprobe */
		fd = open(UPROBE_EVENTS, O_WRONLY | O_APPEND);
		if (fd != -1) {
			rc = dprintf(fd, "p:" GROUP_FMT "/%s %s\n",
				     GROUP_DATA, prp->desc->prb, spec);
			close(fd);
		}
		dt_free(dtp, spec);
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

		rc = tp_event_info(dtp, f, 0, datap, NULL, NULL);
		fclose(f);
	}

	/* attach BPF program to the probe */
	return tp_attach(dtp, prp, bpf_fd);
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
static void probe_fini(dtrace_hdl_t *dtp, const dt_probe_t *prp)
{
	int	fd;

	tp_probe_fini(dtp, prp);

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
	.probe_destroy	= &tp_probe_destroy,
	.probe_fini	= &probe_fini,
};
