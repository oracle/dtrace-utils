/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * The core probe provider for DTrace for the BEGIN, END, and ERROR probes.
 */
#include <assert.h>
#include <string.h>

#include <bpf_asm.h>

#include "dt_provider.h"
#include "dt_probe.h"

static const char		prvname[] = "dtrace";
static const char		modname[] = "";
static const char		funname[] = "";

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
	int		n = 0;


	prv = dt_provider_create(dtp, prvname, &dt_dtrace, &pattr);
	if (prv == NULL)
		return 0;

	if (dt_probe_insert(dtp, prv, prvname, modname, funname, "BEGIN"))
		n++;
	if (dt_probe_insert(dtp, prv, prvname, modname, funname, "END"))
		n++;
	if (dt_probe_insert(dtp, prv, prvname, modname, funname, "ERROR"))
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
 * The trampoline will populate a dt_bpf_context struct and then call the
 * function that implements tha compiled D clause.  It returns the value that
 * it gets back from that function.
 */
static void trampoline(dt_pcb_t *pcb, int haspred)
{
	int		i;
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	struct bpf_insn	instr;
	uint_t		lbl_exit = dt_irlist_label(dlp);
	dt_ident_t	*idp;

#define DCTX_FP(off)	(-(ushort_t)DCTX_SIZE + (ushort_t)(off))

	/*
	 * int dt_dtrace(dt_pt_regs *regs)
	 * {
	 *     struct dt_bpf_context	dctx;
	 *
	 *     memset(&dctx, 0, sizeof(dctx));
	 *
	 *     dctx.epid = EPID;
	 *     (we clear dctx.pad and dctx.fault because of the memset above)
	 */
	idp = dt_dlib_get_var(pcb->pcb_hdl, "EPID");
	assert(idp != NULL);
	instr = BPF_STORE_IMM(BPF_W, BPF_REG_FP, DCTX_FP(DCTX_EPID), -1);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	dlp->dl_last->di_extern = idp;
	instr = BPF_STORE_IMM(BPF_W, BPF_REG_FP, DCTX_FP(DCTX_PAD), 0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE_IMM(BPF_DW, BPF_REG_FP, DCTX_FP(DCTX_FAULT), 0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

	/*
	 *     dctx.regs = *regs;
	 */
	for (i = 0; i < sizeof(dt_pt_regs); i += 8) {
		instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_1, i);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		instr = BPF_STORE(BPF_DW, BPF_REG_FP, DCTX_FP(DCTX_REGS) + i,
				  BPF_REG_0);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	}

	/*
	 *     dctx.argv[0] = PT_REGS_PARAM1(regs);
	 *     dctx.argv[1] = PT_REGS_PARAM2(regs);
	 *     dctx.argv[2] = PT_REGS_PARAM3(regs);
	 *     dctx.argv[3] = PT_REGS_PARAM4(regs);
	 *     dctx.argv[4] = PT_REGS_PARAM5(regs);
	 *     dctx.argv[5] = PT_REGS_PARAM6(regs);
	 */
	instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_1, PT_REGS_ARG0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE(BPF_DW, BPF_REG_FP, DCTX_FP(DCTX_ARG(0)), BPF_REG_0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_1, PT_REGS_ARG1);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE(BPF_DW, BPF_REG_FP, DCTX_FP(DCTX_ARG(1)), BPF_REG_0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_1, PT_REGS_ARG2);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE(BPF_DW, BPF_REG_FP, DCTX_FP(DCTX_ARG(2)), BPF_REG_0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_1, PT_REGS_ARG3);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE(BPF_DW, BPF_REG_FP, DCTX_FP(DCTX_ARG(3)), BPF_REG_0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_1, PT_REGS_ARG4);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE(BPF_DW, BPF_REG_FP, DCTX_FP(DCTX_ARG(4)), BPF_REG_0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_1, PT_REGS_ARG5);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE(BPF_DW, BPF_REG_FP, DCTX_FP(DCTX_ARG(5)), BPF_REG_0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

	/*
	 *     (we clear dctx.argv[6] and on because of the memset above)
	 */
	for (i = 6; i < sizeof(((struct dt_bpf_context *)0)->argv) / 8; i++) {
		instr = BPF_STORE_IMM(BPF_DW, BPF_REG_FP, DCTX_FP(DCTX_ARG(i)),
				      0);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	}

	/*
	 * We know the BPF context (regs) is in %r1.  Since we will be passing
	 * the DTrace context (dctx) as 2nd argument to dt_predicate() (if
	 * there is a predicate) and dt_program, we need it in %r2.
	 */
	instr = BPF_MOV_REG(BPF_REG_2, BPF_REG_FP);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, DCTX_FP(0));
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

	/*
	 *     if (haspred) {
	 *	   rc = dt_predicate(regs, dctx);
	 *	   if (rc == 0) goto exit;
	 *     }
	 */
	if (haspred) {
		/*
		 * Save the BPF context (regs) and DTrace context (dctx) in %r6
		 * and %r7 respectively because the BPF verifier will mark %r1
		 * through %r5 unknown after we call dt_predicate (even if we
		 * do not clobber them).
		 */
		instr = BPF_MOV_REG(BPF_REG_6, BPF_REG_1);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		instr = BPF_MOV_REG(BPF_REG_7, BPF_REG_2);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

		idp = dt_dlib_get_func(pcb->pcb_hdl, "dt_predicate");
		assert(idp != NULL);
		instr = BPF_CALL_FUNC(idp->di_id);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		dlp->dl_last->di_extern = idp;
		instr = BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_0, 0, lbl_exit);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

		/*
		 * Restore BPF context (regs) and DTrace context (dctx) from
		 * %r6 and %r7 into %r1 and %r2 respectively.
		 */
		instr = BPF_MOV_REG(BPF_REG_1, BPF_REG_6);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		instr = BPF_MOV_REG(BPF_REG_2, BPF_REG_7);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	}

	/*
	 *     rc = dt_program(regs, dctx);
	 */
	idp = dt_dlib_get_func(pcb->pcb_hdl, "dt_program");
	assert(idp != NULL);
	instr = BPF_CALL_FUNC(idp->di_id);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	dlp->dl_last->di_extern = idp;

	/*
	 * exit:
	 *     return rc;
	 * }
	 */
	instr = BPF_RETURN();
	dt_irlist_append(dlp, dt_cg_node_alloc(lbl_exit, instr));
}

dt_provimpl_t	dt_dtrace = {
	.name		= prvname,
	.prog_type	= BPF_PROG_TYPE_KPROBE,
	.populate	= &populate,
	.trampoline	= &trampoline,
};
