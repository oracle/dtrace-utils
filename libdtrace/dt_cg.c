/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <sys/types.h>

#include <stdlib.h>
#include <setjmp.h>
#include <assert.h>
#include <errno.h>

#include <dt_impl.h>
#include <dt_dis.h>
#include <dt_dctx.h>
#include <dt_cg.h>
#include <dt_grammar.h>
#include <dt_parser.h>
#include <dt_printf.h>
#include <dt_provider.h>
#include <dt_probe.h>
#include <dt_varint.h>
#include <bpf_asm.h>

static void dt_cg_node(dt_node_t *, dt_irlist_t *, dt_regset_t *);

/*
 * Generate the generic prologue of the trampoline BPF program.
 *
 * The trampoline BPF program is attached to a kernel probe event and it is
 * executed when the probe triggers.  The trampoline must satisfy the following
 * signature:
 *
 *	int dt_dtrace(const void *ctx)
 *
 * The trampoline prologue will populate the dt_dctx_t structure on the stack.
 *
 * The caller can depend on the following registers being set when this
 * function returns:
 *
 *	%r7 contains a pointer to dctx->mst
 *	%r8 contains a pointer to dctx->ctx
 */
void
dt_cg_tramp_prologue_act(dt_pcb_t *pcb, dt_activity_t act)
{
	dtrace_hdl_t	*dtp = pcb->pcb_hdl;
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	dt_ident_t	*mem = dt_dlib_get_map(dtp, "mem");
	dt_ident_t	*state = dt_dlib_get_map(dtp, "state");
	dt_ident_t	*prid = dt_dlib_get_var(pcb->pcb_hdl, "PRID");
	uint_t		lbl_exit = pcb->pcb_exitlbl;

	assert(mem != NULL);
	assert(state != NULL);
	assert(prid != NULL);

	/*
	 * On input, %r1 is the BPF context.
	 *
	 * int dt_dtrace(void *ctx)	//     (%r1 = pointer to BPF context)
	 * {
	 *	dt_dctx_t	dctx;
	 *	uint32_t	key;
	 *	uintptr_t	rc;
	 *	char		*buf;
	 *
	 *				//     (%r8 = pointer to BPF context)
	 *				// mov %r8, %r1
	 *	dctx.ctx = ctx;		// stdw [%fp + DCTX_FP(DCTX_CTX)], %r8
	 */
	emit(dlp,  BPF_MOV_REG(BPF_REG_8, BPF_REG_1));
	emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_FP, DCTX_FP(DCTX_CTX), BPF_REG_8));

	/*
	 *	key = DT_STATE_ACTIVITY;// stw [%fp + DCTX_FP(DCTX_ACT)],
	 *				//		DT_STATE_ACTIVITY
	 *	rc = bpf_map_lookup_elem(&state, &key);
	 *				// lddw %r1, &state
	 *				// mov %r2, %fp
	 *				// add %r2, DCTX_FP(DCTX_ACT)
	 *				// call bpf_map_lookup_elem
	 *				//     (%r1 ... %r5 clobbered)
	 *				//     (%r0 = map value)
	 *	if (rc == 0)		// jeq %r0, 0, lbl_exit
	 *		goto exit;
	 *	if (*rc != act)		// ldw %r1, [%r0 + 0]
	 *		goto exit;	// jne %r1, act, lbl_exit
	 *
	 *	dctx.act = rc;		// stdw [%fp + DCTX_FP(DCTX_ACT)], %r0
	 */
	emit(dlp,  BPF_STORE_IMM(BPF_W, BPF_REG_FP, DCTX_FP(DCTX_ACT), DT_STATE_ACTIVITY));
	dt_cg_xsetx(dlp, state, DT_LBL_NONE, BPF_REG_1, state->di_id);
	emit(dlp,  BPF_MOV_REG(BPF_REG_2, BPF_REG_FP));
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, DCTX_FP(DCTX_ACT)));
	emit(dlp,  BPF_CALL_HELPER(BPF_FUNC_map_lookup_elem));
	emit(dlp,  BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_0, 0, lbl_exit));
	emit(dlp,  BPF_LOAD(BPF_W, BPF_REG_1, BPF_REG_0, 0));
	emit(dlp,  BPF_BRANCH_IMM(BPF_JNE, BPF_REG_1, act, lbl_exit));
	emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_FP, DCTX_FP(DCTX_ACT), BPF_REG_0));

	/*
	 *	key = 0;		// stw [%fp + DCTX_FP(DCTX_MST)], 0
	 *	rc = bpf_map_lookup_elem(&mem, &key);
	 *				// lddw %r1, &mem
	 *				// mov %r2, %fp
	 *				// add %r2, DCTX_FP(DCTX_MST)
	 *				// call bpf_map_lookup_elem
	 *				//     (%r1 ... %r5 clobbered)
	 *				//     (%r0 = 'mem' BPF map value)
	 *	if (rc == 0)		// jeq %r0, 0, lbl_exit
	 *		goto exit;
	 *				//     (%r0 = map value)
	 *				//     (%r7 = pointer to dt_mstate_t)
	 *				// mov %r7, %r0
	 *	dctx.mst = rc;		// stdw [%fp + DCTX_FP(DCTX_MST)], %r7
	 *	dctx.mst->prid = PRID;	// stw [%r7 + DMST_PRID], PRID
	 *	dctx.mst->syscall_errno = 0;
	 *				// stw [%r7 + DMST_ERRNO], 0
	 */
	emit(dlp,  BPF_STORE_IMM(BPF_W, BPF_REG_FP, DCTX_FP(DCTX_MST), 0));
	dt_cg_xsetx(dlp, mem, DT_LBL_NONE, BPF_REG_1, mem->di_id);
	emit(dlp,  BPF_MOV_REG(BPF_REG_2, BPF_REG_FP));
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, DCTX_FP(DCTX_MST)));
	emit(dlp,  BPF_CALL_HELPER(BPF_FUNC_map_lookup_elem));
	emit(dlp,  BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_0, 0, lbl_exit));
	emit(dlp,  BPF_MOV_REG(BPF_REG_7, BPF_REG_0));
	emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_FP, DCTX_FP(DCTX_MST), BPF_REG_7));
	emite(dlp, BPF_STORE_IMM(BPF_W, BPF_REG_7, DMST_PRID, -1), prid);
	emit(dlp,  BPF_STORE_IMM(BPF_W, BPF_REG_7, DMST_ERRNO, 0));

	/*
	 *	buf = rc + roundup(sizeof(dt_mstate_t), 8);
	 *				// add %r0, roundup(
	 *						sizeof(dt_mstate_t), 8)
	 *	*((uint64_t *)&buf[0]) = 0;
	 *				// stdw [%r0 + 0], 0
	 *	buf += 8;		// add %r0, 8
	 *				//     (%r0 = pointer to buffer space)
	 *	dctx.buf = buf;		// stdw [%fp + DCTX_FP(DCTX_BUF)], %r0
	 */
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_0, roundup(sizeof(dt_mstate_t), 8)));
	emit(dlp,  BPF_STORE_IMM(BPF_DW, BPF_REG_0, 0, 0));
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_0, 8));
	emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_FP, DCTX_FP(DCTX_BUF), BPF_REG_0));

	/*
	 * Store pointer to BPF map "name" in the DTrace context field "fld" at
	 * "offset".
	 *
	 * key = 0;		// stw [%fp + DCTX_FP(offset)], 0
	 * rc = bpf_map_lookup_elem(&name, &key);
	 *			// lddw %r1, &name
	 *			// mov %r2, %fp
	 *			// add %r2, DCTX_FP(offset)
	 *			// call bpf_map_lookup_elem
	 *			//     (%r1 ... %r5 clobbered)
	 *			//     (%r0 = name BPF map value)
	 * if (rc == 0)		// jeq %r0, 0, lbl_exit
	 *	goto exit;
	 *			//     (%r0 = pointer to map value)
	 * dctx.fld = rc;	// stdw [%fp + DCTX_FP(offset)], %r0
	 */
#define DT_CG_STORE_MAP_PTR(name, offset) \
	do { \
		dt_ident_t *idp = dt_dlib_get_map(dtp, name); \
		\
		assert(idp != NULL); \
		emit(dlp, BPF_STORE_IMM(BPF_W, BPF_REG_FP, DCTX_FP(offset), 0)); \
		\
		dt_cg_xsetx(dlp, idp, DT_LBL_NONE, BPF_REG_1, idp->di_id); \
		emit(dlp, BPF_MOV_REG(BPF_REG_2, BPF_REG_FP)); \
		emit(dlp, BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, DCTX_FP(offset))); \
		emit(dlp, BPF_CALL_HELPER(BPF_FUNC_map_lookup_elem)); \
		\
		emit(dlp, BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_0, 0, lbl_exit)); \
		\
		emit(dlp, BPF_STORE(BPF_DW, BPF_REG_FP, DCTX_FP(offset), BPF_REG_0)); \
	} while(0)

	DT_CG_STORE_MAP_PTR("strtab", DCTX_STRTAB);
	if (dt_idhash_datasize(dtp->dt_aggs) > 0)
		DT_CG_STORE_MAP_PTR("aggs", DCTX_AGG);
	if (dt_idhash_datasize(dtp->dt_globals) > 0)
		DT_CG_STORE_MAP_PTR("gvars", DCTX_GVARS);
	if (dtp->dt_maxlvaralloc > 0)
		DT_CG_STORE_MAP_PTR("lvars", DCTX_LVARS);
#undef DT_CG_STORE_MAP_PTR
}

void
dt_cg_tramp_prologue(dt_pcb_t *pcb)
{
	dt_cg_tramp_prologue_act(pcb, DT_ACTIVITY_ACTIVE);
}

/*
 * Clear the content of the 'regs' member of the machine state.
 *
 * The caller must ensure that %r7 contains the value set by the
 * dt_cg_tramp_prologue*() functions.
 */
void
dt_cg_tramp_clear_regs(dt_pcb_t *pcb)
{
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	int		i;

	/*
	 *	memset(&dctx->mst->regs, 0, sizeof(dt_pt_regs);
	 *				// stdw [%7 + DMST_REGS + 0 * 8], 0
	 *				// stdw [%7 + DMST_REGS + 1 * 8], 0
	 *				//     (...)
	 */
	for (i = 0; i < sizeof(dt_pt_regs); i += 8)
		emit(dlp, BPF_STORE_IMM(BPF_DW, BPF_REG_7, DMST_REGS + i, 0));
}

/*
 * Copy the content of a dt_pt_regs structure referenced by the 'rp' argument
 * into the 'regs' member of the machine state.
 *
 * The caller must ensure that %r7 contains the value set by the
 * dt_cg_tramp_prologue*() functions.
 */
void
dt_cg_tramp_copy_regs(dt_pcb_t *pcb, int rp)
{
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	int		i;

	/*
	 *	dctx->mst->regs = *(dt_pt_regs *)rp;
	 */
	for (i = 0; i < sizeof(dt_pt_regs); i += 8) {
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_8, i));
		emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_REGS + i, BPF_REG_0));
	}
}

/*
 * Copy arguments from a dt_pt_regs structure referenced by the 'rp' argument.
 *
 * The caller must ensure that %r7 contains the value set by the
 * dt_cg_tramp_prologue*() functions.
 */
void
dt_cg_tramp_copy_args_from_regs(dt_pcb_t *pcb, int rp)
{
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	int		i;

	/*
	 *	for (i = 0; i < PT_REGS_ARGC; i++)
	 *		dctx->mst->argv[i] = PT_REGS_BPF_ARGi((dt_pt_regs *)rp);
	 *				// lddw %r0, [%rp + PT_REGS_ARGi]
	 *				// stdw [%r7 + DMST_ARG(i)], %r0
	 */
	emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_0, rp, PT_REGS_ARG0));
	emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(0), BPF_REG_0));
	emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_0, rp, PT_REGS_ARG1));
	emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(1), BPF_REG_0));
	emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_0, rp, PT_REGS_ARG2));
	emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(2), BPF_REG_0));
	emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_0, rp, PT_REGS_ARG3));
	emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(3), BPF_REG_0));
	emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_0, rp, PT_REGS_ARG4));
	emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(4), BPF_REG_0));
	emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_0, rp, PT_REGS_ARG5));
	emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(5), BPF_REG_0));
#ifdef PT_REGS_ARG6
	emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_0, rp, PT_REGS_ARG6));
	emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(6), BPF_REG_0));
#endif
#ifdef PT_REGS_ARG7
	emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_0, rp, PT_REGS_ARG7));
	emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(7), BPF_REG_0));
#endif

	/*
	 * Generate code to read the remainder of the arguments from the stack
	 * (if possible).  We (ab)use the %r0 spill slot on the stack to read
	 * values using bpf_probe_read() because we know that it cannot be in
	 * use at this point.
	 *
	 *	for (i = PT_REGS_ARGC;
	 *	     i < ARRAY_SIZE(((dt_mstate_t *)0)->argv); i++) {
	 *		uint64_t	val, tmp;
	 *		int		rc;
	 *		uint64_t	*sp;
	 *
	 *		sp = (uint64_t *)(((dt_pt_regs *)rp)->sp;
	 *		rc = bpf_probe_read(&tmp, sizeof(tmp),
	 *				    &sp[i - PT_REGS_ARGC +
	 *					    PT_REGS_ARGSTKBASE]);
	 *				// mov %r1, %fp
	 *				// add %r1, DT_STK_SPILL(0)
	 *				// mov %r2, sizeof(uint64_t)
	 *				// lddw %r3, [%rp + PT_REGS_SP]
	 *				// add %r3, (i - PT_REGS_ARGC +
	 *						 PT_REGS_ARGSTKBASE) *
	 *					    sizeof(uint64_t)
	 *				// call bpf_probe_read
	 *		val = 0;
	 *				// mov %r1, 0
	 *		if (rc != 0)
	 *			goto store:
	 *				// jne %r0, 0, lbl_store
	 *		val = tmp;
	 *				// lddw %r1, [%rp + DT_STK_SPILL(0)]
	 *
	 *	store:
	 *		dctx->mst->argv[i] = val;
	 *				// store:
	 *				// stdw [%r7 + DMST_ARG(i)], %r1
	 *	}
	 *
	 */
	for (i = PT_REGS_ARGC; i < ARRAY_SIZE(((dt_mstate_t *)0)->argv); i++) {
		uint_t	lbl_store = dt_irlist_label(dlp);

		emit(dlp,  BPF_MOV_REG(BPF_REG_1, BPF_REG_FP));
		emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, DT_STK_SPILL(0)));
		emit(dlp,  BPF_MOV_IMM(BPF_REG_2, sizeof(uint64_t)));
		emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_3, rp, PT_REGS_SP));
		emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_3, (i - PT_REGS_ARGC + PT_REGS_ARGSTKBASE) * sizeof(uint64_t)));
		emit(dlp,  BPF_CALL_HELPER(BPF_FUNC_probe_read));
		emit(dlp,  BPF_MOV_IMM(BPF_REG_1, 0));
		emit(dlp,  BPF_BRANCH_IMM(BPF_JNE, BPF_REG_0, 0, lbl_store));
		emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_1, BPF_REG_FP, DT_STK_SPILL(0)));
		emitl(dlp, lbl_store,
			   BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(i), BPF_REG_1));
	}
}

typedef struct {
	dt_irlist_t	*dlp;
	dt_activity_t	act;
	uint_t		lbl_exit;
} dt_clause_arg_t;

static int
dt_cg_call_clause(dtrace_hdl_t *dtp, dt_ident_t *idp, dt_clause_arg_t *arg)
{
	dt_irlist_t	*dlp = arg->dlp;

	/*
	 *	if (*dctx.act != act)	// ldw %r0, [%fp +
	 *				//	     DCTX_FP(DCTX_ACT)]
	 *		goto exit;	// ldw %r0, [%r0 + 0]
	 *				// jne %r0, act, lbl_exit
	 */
	emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_FP, DCTX_FP(DCTX_ACT)));
	emit(dlp,  BPF_LOAD(BPF_W, BPF_REG_0, BPF_REG_0, 0));
	emit(dlp,  BPF_BRANCH_IMM(BPF_JNE, BPF_REG_0, arg->act, arg->lbl_exit));

	/*
	 *	dt_clause(dctx);	// mov %r1, %fp
	 *				// add %r1, DCTX_FP(0)
	 *				// call clause
	 */
	emit(dlp,  BPF_MOV_REG(BPF_REG_1, BPF_REG_FP));
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, DCTX_FP(0)));
	emite(dlp, BPF_CALL_FUNC(idp->di_id), idp);

	return 0;
}

void
dt_cg_tramp_call_clauses(dt_pcb_t *pcb, const dt_probe_t *prp,
			 dt_activity_t act)
{
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	dt_clause_arg_t	arg = { dlp, act, pcb->pcb_exitlbl };

	dt_probe_clause_iter(pcb->pcb_hdl, prp,
			     (dt_clause_f *)dt_cg_call_clause, &arg);
}

void
dt_cg_tramp_return(dt_pcb_t *pcb)
{
	dt_irlist_t	*dlp = &pcb->pcb_ir;

	/*
	 * exit:
	 *	return 0;		// mov %r0, 0
	 *				// exit
	 * }
	 */
	emitl(dlp, pcb->pcb_exitlbl,
		   BPF_MOV_IMM(BPF_REG_0, 0));
	emit(dlp,  BPF_RETURN());
}

void
dt_cg_tramp_epilogue(dt_pcb_t *pcb)
{
	dt_cg_tramp_call_clauses(pcb, pcb->pcb_probe, DT_ACTIVITY_ACTIVE);
	dt_cg_tramp_return(pcb);
}

void
dt_cg_tramp_epilogue_advance(dt_pcb_t *pcb, dt_activity_t act)
{
	dt_irlist_t	*dlp = &pcb->pcb_ir;

	dt_cg_tramp_call_clauses(pcb, pcb->pcb_probe, act);

	/*
	 *	(*dctx.act)++;		// lddw %r0, [%fp + DCTX_FP(DCTX_ACT)]
	 *				// mov %r1, 1
	 *				// xadd [%r0 + 0], %r1
	 */
	emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_FP, DCTX_FP(DCTX_ACT)));
	emit(dlp, BPF_MOV_IMM(BPF_REG_1, 1));
	emit(dlp, BPF_XADD_REG(BPF_W, BPF_REG_0, 0, BPF_REG_1));

	dt_cg_tramp_return(pcb);
}

static int
dt_cg_tramp_error_call_clause(dtrace_hdl_t *dtp, dt_ident_t *idp,
			      dt_irlist_t *dlp)
{
	/*
	 *	dt_error_#(dctx);	// mov %r1, %r9
	 *				// call dt_error_#
	 */
	emit(dlp,  BPF_MOV_REG(BPF_REG_1, BPF_REG_9));
	emite(dlp, BPF_CALL_FUNC(idp->di_id), idp);

	return 0;
}

/*
 * Generate the trampoline BPF program for the ERROR probe.
 *
 * The trampoline BPF program for the ERROR probe is implemented as a function
 * that calls the ERROR probe clauses one by one.  It must satisfy the
 * signature:
 *
 *	int dt_error(dt_dctx_t *dctx)
 */
void
dt_cg_tramp_error(dt_pcb_t *pcb)
{
	dtrace_hdl_t	*dtp = pcb->pcb_hdl;
	dt_irlist_t	*dlp = &pcb->pcb_ir;

	/*
	 * int dt_error(dt_dctx_t *dctx)
	 *				//     (%r1 = pointer to dt_dctx_t)
	 * {
	 *	int	rc;
	 *				//     (%r9 = reserved reg for dctx)
	 *				// mov %r9, %r1
	 */
	TRACE_REGSET("Trampoline: Begin");
	emit(dlp, BPF_MOV_REG(BPF_REG_9, BPF_REG_1));

	dt_probe_clause_iter(dtp, dtp->dt_error,
			     (dt_clause_f *)dt_cg_tramp_error_call_clause, dlp);

	emit(dlp, BPF_MOV_IMM(BPF_REG_0, 0));
	emit(dlp, BPF_RETURN());
	TRACE_REGSET("Trampoline: Begin");
}

/*
 * Generate the function prologue.
 *
 * The function signature will be:
 *
 *	int dt_program(dt_dctx_t *dctx)
 *
 * The prologue will:
 *
 *	1. Store the base pointer to the output data buffer in %r9.
 *	2. Initialize the machine state (dctx->mst).
 *	3. Store the epid and tag at [%r9 + 0] and [%r9 + 4] respectively.
 *	4. Evaluate the predicate expression and return if false.
 *
 * The dt_program() function will always return 0.
 */
static void
dt_cg_prologue(dt_pcb_t *pcb, dt_node_t *pred)
{
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	dt_ident_t	*epid = dt_dlib_get_var(pcb->pcb_hdl, "EPID");
	dt_ident_t	*clid = dt_dlib_get_var(pcb->pcb_hdl, "CLID");

	assert(epid != NULL);
	assert(clid != NULL);

	/*
	 * void dt_program(dt_dctx_t *dctx)
	 *				//     (%r1 = pointer to dt_dctx_t)
	 * {
	 *	int	rc;
	 *	char	*buf;		//     (%r9 = reserved reg for buf)
	 *
	 *				// stdw [%fp + DT_STK_DCTX], %r1
	 */
	TRACE_REGSET("Prologue: Begin");
	emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_FP, DT_STK_DCTX, BPF_REG_1));

	/*
	 *	buf = dctx->buf;	// lddw %r0, [%fp + DT_STK_DCTX]
	 *				// lddw %r9, [%r0 + DCTX_BUF]
	 */
	emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_FP, DT_STK_DCTX));
	emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_9, BPF_REG_0, DCTX_BUF));

	/*
	 *	dctx->mst->fault = 0;	// lddw %r0, [%r0 + DCTX_MST]
	 *				// stdw [%r0 + DMST_FAULT], 0
	 *	dctx->mst->tstamp = 0;	// stdw [%r0 + DMST_TSTAMP], 0
	 *	dctx->mst->epid = EPID;	// stw [%r0 + DMST_EPID], EPID
	 *	dctx->mst->clid = CLID;	// stw [%r0 + DMST_CLID], CLID
	 *	*((uint32_t *)&buf[0]) = EPID;
	 *				// stw [%r9 + 0], EPID
	 */
	emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_0, DCTX_MST));
	emit(dlp,  BPF_STORE_IMM(BPF_DW, BPF_REG_0, DMST_FAULT, 0));
	emit(dlp,  BPF_STORE_IMM(BPF_DW, BPF_REG_0, DMST_TSTAMP, 0));
	emite(dlp, BPF_STORE_IMM(BPF_W, BPF_REG_0, DMST_EPID, -1), epid);
	emite(dlp, BPF_STORE_IMM(BPF_W, BPF_REG_0, DMST_CLID, -1), clid);
	emite(dlp, BPF_STORE_IMM(BPF_W, BPF_REG_9, 0, -1), epid);

	/*
	 *	dctx->mst->tag = 0;	// stw [%r0 + DMST_TAG], 0
	 *	*((uint32_t *)&buf[4]) = 0;
	 *				// stw [%r9 + 4], 0
	 */
	emit(dlp,  BPF_STORE_IMM(BPF_W, BPF_REG_0, DMST_TAG, 0));
	emit(dlp,  BPF_STORE_IMM(BPF_W, BPF_REG_9, 4, 0));

	/*
	 * If there is a predicate:
	 *
	 *	rc = predicate;		//     (evaluate predicate into %rX)
	 *	if (rc == 0)		// jeq %rX, 0, pcb->pcb_exitlbl
	 *		goto exit;
	 */
	if (pred != NULL) {
		TRACE_REGSET("    Pred: Begin");
		dt_cg_node(pred, &pcb->pcb_ir, pcb->pcb_regs);
		emit(dlp, BPF_BRANCH_IMM(BPF_JEQ, pred->dn_reg, 0, pcb->pcb_exitlbl));
		dt_regset_free(pcb->pcb_regs, pred->dn_reg);
		TRACE_REGSET("    Pred: End  ");
	}

	TRACE_REGSET("Prologue: End  ");

	/*
	 * Account for 32-bit epid (at offset 0) and 32-bit tag (at offset 4).
	 */
	pcb->pcb_bufoff += 2 * sizeof(uint32_t);
}

/*
 * Generate the function epilogue:
 *	4. Submit the buffer to the perf event output buffer for the current
 *	   cpu, if this is a data recording action..
 *	5. Return 0
 * }
 */
static void
dt_cg_epilogue(dt_pcb_t *pcb)
{
	dt_irlist_t	*dlp = &pcb->pcb_ir;

	TRACE_REGSET("Epilogue: Begin");

	/*
	 * Output the buffer if:
	 *   - data-recording action, or
	 *   - default action (no clause specified)
	 */
	if (pcb->pcb_stmt->dtsd_clauseflags & DT_CLSFLAG_DATAREC) {
		dt_ident_t *buffers = dt_dlib_get_map(pcb->pcb_hdl, "buffers");

		assert(buffers != NULL);

		/*
		 *	bpf_perf_event_output(dctx->ctx, &buffers,
		 *			      BPF_F_CURRENT_CPU,
		 *			      buf - 4, bufoff + 4);
		 *				// lddw %r1, [%fp + DT_STK_DCTX]
		 *				// lddw %r1, [%r1 + DCTX_CTX]
		 *				// lddw %r2, &buffers
		 *				// lddw %r3, BPF_F_CURRENT_CPU
		 *				// mov %r4, %r9
		 *				// add %r4, -4
		 *				// mov %r5, pcb->pcb_bufoff
		 *				// add %r5, 4
		 *				// call bpf_perf_event_output
		 *
		 */
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_1, BPF_REG_FP, DT_STK_DCTX));
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_1, BPF_REG_1, DCTX_CTX));
		dt_cg_xsetx(dlp, buffers, DT_LBL_NONE, BPF_REG_2, buffers->di_id);
		dt_cg_xsetx(dlp, NULL, DT_LBL_NONE, BPF_REG_3, BPF_F_CURRENT_CPU);
		emit(dlp, BPF_MOV_REG(BPF_REG_4, BPF_REG_9));
		emit(dlp, BPF_ALU64_IMM(BPF_ADD, BPF_REG_4, -4));
		emit(dlp, BPF_MOV_IMM(BPF_REG_5, pcb->pcb_bufoff));
		emit(dlp, BPF_ALU64_IMM(BPF_ADD, BPF_REG_5, 4));
		emit(dlp, BPF_CALL_HELPER(BPF_FUNC_perf_event_output));
	}

	/*
	 * exit:
	 *	return 0;		// mov %r0, 0
	 *				// exit
	 * }
	 */
	emitl(dlp, pcb->pcb_exitlbl,
		   BPF_MOV_IMM(BPF_REG_0, 0));
	emit(dlp,  BPF_RETURN());
	TRACE_REGSET("Epilogue: End  ");
}

/*
 * Generate code for a fault condition.  A call is made to dt_probe_error() to
 * set the fault information.
 */
static void
dt_cg_probe_error(dt_pcb_t *pcb, uint32_t off, uint32_t fault, uint64_t illval)
{
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	dt_regset_t	*drp = pcb->pcb_regs;
	dt_ident_t	*idp = dt_dlib_get_func(yypcb->pcb_hdl,
						"dt_probe_error");

	assert(idp != NULL);

	/*
	 *	dt_probe_error(
	 *		dctx,		// lddw %r1, %fp, DT_STK_DCT
	 *		off,		// mov %r2, off
	 *		fault,		// mov %r3, fault
	 *		illval);	// mov %r4, illval
	 *				// call dt_probe_error
	 *	return;			// exit
	 */
	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_1, BPF_REG_FP, DT_STK_DCTX));
	emit(dlp,  BPF_MOV_IMM(BPF_REG_2, off));
	emit(dlp,  BPF_MOV_IMM(BPF_REG_3, fault));
	emit(dlp,  BPF_MOV_IMM(BPF_REG_4, illval));
	dt_regset_xalloc(drp, BPF_REG_0);
	emite(dlp, BPF_CALL_FUNC(idp->di_id), idp);
	dt_regset_free_args(drp);
	dt_regset_free(drp, BPF_REG_0);
	emit(dlp,  BPF_RETURN());
}

/*
 * Generate code to validate that the value in the given register 'reg' is not
 * the NULL pointer.
 */
static void
dt_cg_check_notnull(dt_irlist_t *dlp, dt_regset_t *drp, int reg)
{
	uint_t	lbl_notnull = dt_irlist_label(dlp);

	emit(dlp,  BPF_BRANCH_IMM(BPF_JNE, reg, 0, lbl_notnull));
	dt_cg_probe_error(yypcb, -1, DTRACEFLT_BADADDR, 0);
	emitl(dlp, lbl_notnull,
		   BPF_NOP());
}

/*
 * Check whether mst->fault indicates a fault was triggered.  If so, abort the
 * current clause by means of a straight jump to the exit labal.
 */
static void
dt_cg_check_fault(dt_pcb_t *pcb)
{
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	dt_regset_t	*drp = pcb->pcb_regs;
	int		reg = dt_regset_alloc(drp);
	uint_t		lbl_ok = dt_irlist_label(dlp);

	if (reg == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	emit(dlp,  BPF_LOAD(BPF_DW, reg, BPF_REG_FP, DT_STK_DCTX));
	emit(dlp,  BPF_LOAD(BPF_DW, reg, reg, DCTX_MST));
	emit(dlp,  BPF_LOAD(BPF_DW, reg, reg, DMST_FAULT));
	emit(dlp,  BPF_BRANCH_IMM(BPF_JEQ, reg, 0, lbl_ok));
	emit(dlp,  BPF_MOV_IMM(BPF_REG_0, 0));
	emit(dlp,  BPF_RETURN());
	emitl(dlp, lbl_ok,
		   BPF_NOP());
	dt_regset_free(drp, reg);
}
/*
 * Generate an instruction sequence to fill a gap in the output buffer with 0
 * values.  This is used to ensure that there are no uninitialized bytes in the
 * output buffer that could result from alignment requirements for data
 * records.
 */
static void
dt_cg_fill_gap(dt_pcb_t *pcb, int gap)
{
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	uint_t		off = pcb->pcb_bufoff;

	if (gap & 1) {
		emit(dlp, BPF_STORE_IMM(BPF_B, BPF_REG_9, off, 0));
		off += 1;
	}
	if (gap & 2) {
		emit(dlp, BPF_STORE_IMM(BPF_H, BPF_REG_9, off, 0));
		off += 2;
	}
	if (gap & 4)
		emit(dlp, BPF_STORE_IMM(BPF_W, BPF_REG_9, off, 0));
}

static void
dt_cg_memcpy(dt_irlist_t *dlp, dt_regset_t *drp, int dst, int src, size_t size)
{
	dt_ident_t	*idp = dt_dlib_get_func(yypcb->pcb_hdl, "dt_memcpy");

	assert(idp != NULL);
	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	emit(dlp,  BPF_MOV_REG(BPF_REG_1, dst));
	emit(dlp,  BPF_MOV_REG(BPF_REG_2, src));
	emit(dlp,  BPF_MOV_IMM(BPF_REG_3, size));
	dt_regset_xalloc(drp, BPF_REG_0);
	emite(dlp, BPF_CALL_FUNC(idp->di_id), idp);
	dt_regset_free_args(drp);
	/* FIXME: check BPF_REG_0 for error? */
	dt_regset_free(drp, BPF_REG_0);
}

static void
dt_cg_strlen(dt_irlist_t *dlp, dt_regset_t *drp, int dst, int src)
{
	dt_ident_t	*idp = dt_dlib_get_func(yypcb->pcb_hdl, "dt_vint2int");
	size_t		size = yypcb->pcb_hdl->dt_options[DTRACEOPT_STRSIZE];
	uint_t		lbl_ok = dt_irlist_label(dlp);

	assert(idp != NULL);
	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	emit(dlp,  BPF_MOV_REG(BPF_REG_1, src));
	dt_regset_xalloc(drp, BPF_REG_0);
	emite(dlp, BPF_CALL_FUNC(idp->di_id), idp);
	dt_regset_free_args(drp);
	emit(dlp,  BPF_BRANCH_IMM(BPF_JLE, BPF_REG_0, size, lbl_ok));
	emit(dlp,  BPF_MOV_IMM(BPF_REG_0, size));
	emitl(dlp, lbl_ok,
		   BPF_MOV_REG(dst, BPF_REG_0));
	dt_regset_free(drp, BPF_REG_0);
}

static void
dt_cg_spill_store(int reg)
{
	dt_irlist_t	*dlp = &yypcb->pcb_ir;

	emit(dlp, BPF_STORE(BPF_DW, BPF_REG_FP, DT_STK_SPILL(reg), reg));
}

static void
dt_cg_spill_load(int reg)
{
	dt_irlist_t	*dlp = &yypcb->pcb_ir;

	emit(dlp, BPF_LOAD(BPF_DW, reg, BPF_REG_FP, DT_STK_SPILL(reg)));
}

static const uint_t	ldstw[] = {
					0,
					BPF_B,	BPF_H,	0, BPF_W,
					0,	0,	0, BPF_DW,
				};

static int
dt_cg_store_val(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind,
		dt_pfargv_t *pfp, int arg)
{
	dtrace_diftype_t	vtype;
	dtrace_hdl_t		*dtp = pcb->pcb_hdl;
	dt_irlist_t		*dlp = &pcb->pcb_ir;
	dt_regset_t		*drp = pcb->pcb_regs;
	uint_t			off;
	size_t			size;

	/*
	 * Special case for aggregations: we store the aggregation id.  We
	 * cannot just generate code for the dnp node because it has no type.
	 */
	if (dnp->dn_kind == DT_NODE_AGG) {
		if ((dnp->dn_reg = dt_regset_alloc(drp)) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
		emit(dlp, BPF_MOV_IMM(dnp->dn_reg, dnp->dn_ident->di_id));
		size = sizeof(dnp->dn_ident->di_id);
	} else {
		dt_cg_node(dnp, &pcb->pcb_ir, drp);
		dt_node_diftype(dtp, dnp, &vtype);
		size = vtype.dtdt_size;
	}

	if (kind == DTRACEACT_USYM ||
	    kind == DTRACEACT_UMOD ||
	    kind == DTRACEACT_UADDR) {
		off = dt_rec_add(dtp, dt_cg_fill_gap, kind, 16, 8, NULL, arg);

		/* preface the value with the user process tgid */
		if (dt_regset_xalloc_args(drp) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
		dt_regset_xalloc(drp, BPF_REG_0);
		emit(dlp,  BPF_CALL_HELPER(BPF_FUNC_get_current_pid_tgid));
		dt_regset_free_args(drp);
		emit(dlp,  BPF_ALU64_IMM(BPF_AND, BPF_REG_0, 0xffffffff));
		emit(dlp, BPF_STORE(BPF_DW, BPF_REG_9, off, BPF_REG_0));
		dt_regset_free(drp, BPF_REG_0);

		/* then store the value */
		emit(dlp, BPF_STORE(BPF_DW, BPF_REG_9, off + 8, dnp->dn_reg));
		dt_regset_free(drp, dnp->dn_reg);

		return 0;
	}

	if (dt_node_is_scalar(dnp) || dt_node_is_float(dnp) ||
	    dnp->dn_kind == DT_NODE_AGG) {
		off = dt_rec_add(dtp, dt_cg_fill_gap, kind,
				 size, size, pfp, arg);

		assert(size > 0 && size <= 8 && (size & (size - 1)) == 0);

		emit(dlp, BPF_STORE(ldstw[size], BPF_REG_9, off, dnp->dn_reg));
		dt_regset_free(drp, dnp->dn_reg);

		return 0;
	} else if (dt_node_is_string(dnp)) {
		dt_ident_t	*idp;
		uint_t		vcopy = dt_irlist_label(dlp);
		int		reg = dt_regset_alloc(drp);

		off = dt_rec_add(pcb->pcb_hdl, dt_cg_fill_gap, kind,
				 size, 1, pfp, arg);

		/* Retrieve the length of the string.  */
		dt_cg_strlen(dlp, drp, reg, dnp->dn_reg);

		if (dt_regset_xalloc_args(drp) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

		/* Determine the number of bytes used for the length. */
		emit(dlp,   BPF_MOV_REG(BPF_REG_1, reg));
		idp = dt_dlib_get_func(yypcb->pcb_hdl, "dt_vint_size");
		assert(idp != NULL);
		dt_regset_xalloc(drp, BPF_REG_0);
		emite(dlp,  BPF_CALL_FUNC(idp->di_id), idp);

		/* Add length of the string (adjusted for terminating byte). */
		emit(dlp,   BPF_ALU64_IMM(BPF_ADD, reg, 1));
		emit(dlp,   BPF_ALU64_REG(BPF_ADD, BPF_REG_0, reg));
		dt_regset_free(drp, reg);

		/*
		 * Copy string data (varint length + string content) to the
		 * output buffer at [%r9 + off].  The amount of bytes copied is
		 * the lesser of the data size and the maximum string size.
		 */
		emit(dlp,   BPF_MOV_REG(BPF_REG_1, BPF_REG_9));
		emit(dlp,   BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, off));
		emit(dlp,   BPF_MOV_REG(BPF_REG_2, dnp->dn_reg));
		dt_regset_free(drp, dnp->dn_reg);
		emit(dlp,   BPF_MOV_REG(BPF_REG_3, BPF_REG_0));
		dt_regset_free(drp, BPF_REG_0);
		emit(dlp,   BPF_BRANCH_IMM(BPF_JLT, BPF_REG_3, size, vcopy));
		emit(dlp,   BPF_MOV_IMM(BPF_REG_3, size));
		idp = dt_dlib_get_func(yypcb->pcb_hdl, "dt_memcpy");
		assert(idp != NULL);
		dt_regset_xalloc(drp, BPF_REG_0);
		emitle(dlp, vcopy,
			    BPF_CALL_FUNC(idp->di_id), idp);
		dt_regset_free_args(drp);
		dt_regset_free(drp, BPF_REG_0);

		return 0;
	}

	return -1;
}

#ifdef FIXME
/*
 * Utility function to determine if a given action description is destructive.
 * The DIFOFLG_DESTRUCTIVE bit is set for us by the DIF assembler (see dt_as.c).
 */
static int
dt_action_destructive(const dtrace_actdesc_t *ap)
{
	return (DTRACEACT_ISDESTRUCTIVE(ap->dtad_kind) ||
		(ap->dtad_kind == DTRACEACT_DIFEXPR &&
		 (ap->dtad_difo->dtdo_flags & DIFOFLG_DESTRUCTIVE)));
}
#endif

static void
dt_cg_clsflags(dt_pcb_t *pcb, dtrace_actkind_t kind, const dt_node_t *dnp)
{
	int		*cfp = &pcb->pcb_stmt->dtsd_clauseflags;

	if (DTRACEACT_ISDESTRUCTIVE(kind))
		*cfp |= DT_CLSFLAG_DESTRUCT;

	if (kind == DTRACEACT_COMMIT) {
		if (*cfp & DT_CLSFLAG_COMMIT)
			dnerror(dnp, D_COMM_COMM,
			    "commit( ) may not follow commit( )\n");
		if (*cfp & (DT_CLSFLAG_DATAREC | DT_CLSFLAG_AGGREGATION))
			dnerror(dnp, D_COMM_DREC, "commit( ) may "
			    "not follow data-recording action(s)\n");

		*cfp |= DT_CLSFLAG_COMMIT;
		return;
	}

	if (kind == DTRACEACT_SPECULATE) {
		if (*cfp & DT_CLSFLAG_SPECULATE)
			dnerror(dnp, D_SPEC_SPEC,
			    "speculate( ) may not follow speculate( )\n");

		if (*cfp & DT_CLSFLAG_COMMIT)
			dnerror(dnp, D_SPEC_COMM,
			    "speculate( ) may not follow commit( )\n");

		if (*cfp & (DT_CLSFLAG_DATAREC | DT_CLSFLAG_AGGREGATION))
			dnerror(dnp, D_SPEC_DREC, "speculate( ) may "
			    "not follow data-recording action(s)\n");

		*cfp |= DT_CLSFLAG_SPECULATE;
		return;
	}

	if (DTRACEACT_ISAGG(kind)) {
		if (*cfp & DT_CLSFLAG_COMMIT)
			dnerror(dnp, D_AGG_COMM,
			    "aggregating actions may not follow commit( )\n");
		if (*cfp & DT_CLSFLAG_SPECULATE)
			dnerror(dnp, D_AGG_SPEC, "aggregating actions "
			    "may not follow speculate( )\n");

		*cfp |= DT_CLSFLAG_AGGREGATION;
		return;
	}

	if (*cfp & DT_CLSFLAG_SPECULATE) {
#ifdef FIXME
		if (dt_action_destructive(ap))
#else
		if (DTRACEACT_ISDESTRUCTIVE(kind))
#endif
			dnerror(dnp, D_ACT_SPEC, "destructive actions "
			    "may not follow speculate( )\n");
		if (kind == DTRACEACT_EXIT)
			dnerror(dnp, D_EXIT_SPEC,
			    "exit( ) may not follow speculate( )\n");
	}

#ifdef FIXME
	if (kind == DTRACEACT_DIFEXPR &&
	    ap->dtad_difo->dtdo_rtype.dtdt_kind == DIF_TYPE_CTF &&
	    ap->dtad_difo->dtdo_rtype.dtdt_size == 0)
		return;
#endif

	if (*cfp & DT_CLSFLAG_COMMIT)
		dnerror(dnp, D_DREC_COMM,
		    "data-recording actions may not follow commit( )\n");

	if (!(*cfp & DT_CLSFLAG_SPECULATE))
		*cfp |= DT_CLSFLAG_DATAREC;
}

static void
dt_cg_act_breakpoint(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind)
{
	dnerror(dnp, D_UNKNOWN, "breakpoint() is not implemented (yet)\n");
	/* FIXME: Needs implementation */
}

static void
dt_cg_act_chill(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind)
{
	dt_cg_node(dnp->dn_args, &pcb->pcb_ir, pcb->pcb_regs);
	dnerror(dnp, D_UNKNOWN, "chill() is not implemented (yet)\n");
	/* FIXME: Needs implementation */
}

static void
dt_cg_act_clear(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind)
{
	dt_node_t	*anp;
	dt_ident_t	*aid;
	char		n[DT_TYPE_NAMELEN];

	anp = dnp->dn_args;
	assert(anp != NULL);
	if (anp->dn_kind != DT_NODE_AGG)
		dnerror(dnp, D_CLEAR_AGGARG,
			"%s( ) argument #1 is incompatible with prototype:\n"
			"\tprototype: aggregation\n\t argument: %s\n",
			dnp->dn_ident->di_name,
			dt_node_type_name(anp, n, sizeof(n)));

	aid = anp->dn_ident;
	if (aid->di_gen == pcb->pcb_hdl->dt_gen &&
	    !(aid->di_flags & DT_IDFLG_MOD))
		dnerror(dnp, D_CLEAR_AGGBAD,
			"undefined aggregation: @%s\n", aid->di_name);

	/*
	 * FIXME: Needs implementation
	 * TODO: Emit code to clear the given aggregation.
	 * DEPENDS ON: How aggregations are implemented using eBPF (hashmap?).
	 * AGGID = aid->di_id
	 */
	dt_cg_store_val(pcb, anp, DTRACEACT_LIBACT, NULL, DT_ACT_CLEAR);
}

/*
 * Signal that the speculation with the given id should be committed to the
 * tracing output.
 *
 * Stores:
 *	integer (4 bytes)		-- speculation ID
 */
static void
dt_cg_act_commit(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind)
{
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	uint_t		off;

	dt_cg_node(dnp->dn_args, &pcb->pcb_ir, pcb->pcb_regs);

	off = dt_rec_add(pcb->pcb_hdl, dt_cg_fill_gap, DTRACEACT_COMMIT,
			 sizeof(uint64_t), sizeof(uint64_t), NULL,
			 DT_ACT_COMMIT);

	emit(dlp, BPF_STORE(BPF_DW, BPF_REG_9, off, BPF_REG_0)); /* FIXME */
}

static void
dt_cg_act_denormalize(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind)
{
	dt_node_t	*anp;
	char		n[DT_TYPE_NAMELEN];
	dt_ident_t	*aid;

	anp = dnp->dn_args;
	assert(anp != NULL);
	if (anp->dn_kind != DT_NODE_AGG)
		dnerror(dnp, D_NORMALIZE_AGGARG,
			"%s( ) argument #1 is incompatible with prototype:\n"
			"\tprototype: aggregation\n\t argument: %s\n",
			dnp->dn_ident->di_name,
			dt_node_type_name(anp, n, sizeof(n)));

	aid = anp->dn_ident;
	if (aid->di_gen == pcb->pcb_hdl->dt_gen &&
	    !(aid->di_flags & DT_IDFLG_MOD))
		dnerror(dnp, D_NORMALIZE_AGGBAD,
			"undefined aggregation: @%s\n", aid->di_name);

	dt_cg_store_val(pcb, anp, DTRACEACT_LIBACT, NULL, DT_ACT_DENORMALIZE);
}

/*
 * Signal that the speculation with the given id should be discarded from the
 * tracing output.
 *
 * Stores:
 *	integer (4 bytes)		-- speculation ID
 */
static void
dt_cg_act_discard(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind)
{
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	uint_t		off;

	dt_cg_node(dnp->dn_args, &pcb->pcb_ir, pcb->pcb_regs);

	off = dt_rec_add(pcb->pcb_hdl, dt_cg_fill_gap, DTRACEACT_DISCARD,
			 sizeof(uint64_t), sizeof(uint64_t), NULL,
			 DT_ACT_DISCARD);

	emit(dlp, BPF_STORE(BPF_DW, BPF_REG_9, off, BPF_REG_0)); /* FIXME */
}

/*
 * Signal that tracing should be terminated with the given return code.  This
 * action also forces the activity state to be set to DRAINING to signal that
 * tracing is ending.
 *
 * Stores:
 *	integer (4 bytes)		-- return code
 */
static void
dt_cg_act_exit(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind)
{
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	dt_ident_t	*state = dt_dlib_get_map(pcb->pcb_hdl, "state");

	assert(state != NULL);

	/* Record the exit code. */
	dt_cg_store_val(pcb, dnp->dn_args, DTRACEACT_EXIT, NULL, DT_ACT_EXIT);

	/*
	 * Force the activity state to DRAINING.
	 *
	 *	key = DT_STATE_ACTIVITY;// stw [%fp + DT_STK_SPILL(0)],
	 *				//		DT_STATE_ACTIVITY
	 *	val = DT_ACTIVITY_DRAINING;
	 *				// stw [%fp + DT_STK_SPILL(1)],
	 *				//		DT_ACTIVITY_DRAINING
	 *	rc = bpf_map_update_elem(&state, &key, &val, BPF_ANY);
	 *				// lddw %r1, &state
	 *				// mov %r2, %fp
	 *				// add %r2, DT_STK_SPILL(0)
	 *				// mov %r3, %fp
	 *				// add %r3, DT_STK_SPILL(1)
	 *				// mov %r4, BPF_ANY
	 *				// call bpf_map_update_elem
	 *				//     (%r1 ... %r5 clobbered)
	 *				//     (%r0 = return code)
	 */
	emit(dlp, BPF_STORE_IMM(BPF_W, BPF_REG_FP, DT_STK_SPILL(0), DT_STATE_ACTIVITY));
	emit(dlp, BPF_STORE_IMM(BPF_W, BPF_REG_FP, DT_STK_SPILL(1), DT_ACTIVITY_DRAINING));
	dt_cg_xsetx(dlp, state, DT_LBL_NONE, BPF_REG_1, state->di_id);
	emit(dlp, BPF_MOV_REG(BPF_REG_2, BPF_REG_FP));
	emit(dlp, BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, DT_STK_SPILL(0)));
	emit(dlp, BPF_MOV_REG(BPF_REG_3, BPF_REG_FP));
	emit(dlp, BPF_ALU64_IMM(BPF_ADD, BPF_REG_3, DT_STK_SPILL(1)));
	emit(dlp, BPF_MOV_IMM(BPF_REG_4, BPF_ANY));
	emit(dlp, BPF_CALL_HELPER(BPF_FUNC_map_update_elem));
}

static void
dt_cg_act_ftruncate(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind)
{
}

static void
dt_cg_act_jstack(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind)
{
}

static void
dt_cg_act_normalize(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind)
{
	dt_node_t	*anp, *normal;
	char		n[DT_TYPE_NAMELEN];
	dt_ident_t	*aid;

	anp = dnp->dn_args;
	assert(anp != NULL);
	if (anp->dn_kind != DT_NODE_AGG)
		dnerror(dnp, D_NORMALIZE_AGGARG,
			"%s( ) argument #1 is incompatible with prototype:\n"
			"\tprototype: aggregation\n\t argument: %s\n",
			dnp->dn_ident->di_name,
			dt_node_type_name(anp, n, sizeof(n)));

	normal = anp->dn_list;
	if (!dt_node_is_scalar(normal))
		dnerror(dnp, D_NORMALIZE_SCALAR,
			"%s( ) argument #2 must be of scalar type\n",
			dnp->dn_ident->di_name);

	aid = anp->dn_ident;
	if (aid->di_gen == pcb->pcb_hdl->dt_gen &&
	    !(aid->di_flags & DT_IDFLG_MOD))
		dnerror(dnp, D_NORMALIZE_AGGBAD,
			"undefined aggregation: @%s\n", aid->di_name);

	dt_cg_store_val(pcb, anp, DTRACEACT_LIBACT, NULL, DT_ACT_NORMALIZE);
	dt_cg_store_val(pcb, normal, DTRACEACT_LIBACT, NULL, DT_ACT_NORMALIZE);
}

static void
dt_cg_act_panic(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind)
{
}

static void
dt_cg_act_pcap(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind)
{
}

static void
dt_cg_act_printa(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind)
{
	dt_node_t	*anp, *proto = NULL;
	dt_pfargv_t	*pfp = NULL;
	const char	*fmt;
	int		argc = 0, argr;
	char		n[DT_TYPE_NAMELEN];
	dt_ident_t	*aid, *fid;

	/* Count the arguments. */
	for (anp = dnp->dn_args; anp != NULL; anp = anp->dn_list)
		argc++;

	/* Get format string (if any). */
	if (dnp->dn_args->dn_kind == DT_NODE_STRING) {
		fmt = dnp->dn_args->dn_string;
		anp = dnp->dn_args->dn_list;
		argr = 2;
	} else {
		fmt = NULL;
		anp = dnp->dn_args;
		argr = 1;
	}

	if (argc < argr)
		dnerror(dnp, D_PRINTA_PROTO,
			"%s( ) prototype mismatch: %d args passed, "
			"%d expected\n", dnp->dn_ident->di_name, argc, argr);

	assert(anp != NULL);

	while (anp != NULL) {
		if (anp->dn_kind != DT_NODE_AGG)
			dnerror(dnp, D_PRINTA_AGGARG,
				"%s( ) argument #%d is incompatible with "
				"prototype:\n\tprototype: aggregation\n"
				"\t argument: %s\n", dnp->dn_ident->di_name,
				argr, dt_node_type_name(anp, n, sizeof(n)));

		aid = anp->dn_ident;
		fid = aid->di_iarg;

		if (aid->di_gen == pcb->pcb_hdl->dt_gen &&
		    !(aid->di_flags & DT_IDFLG_MOD))
			dnerror(dnp, D_PRINTA_AGGBAD,
				"undefined aggregation: @%s\n", aid->di_name);

		/*
		 * If multiple aggregations are specified, their signatures
		 * must match.
		 */
		if (proto != NULL)
			dt_printa_validate(proto, anp);
		else
			proto = anp;

		/*
		 * Validate the format string and the datatypes of the keys, if
		 * there is a format string specified.
		 */
		if (fmt != NULL) {
			yylineno = dnp->dn_line;

			pfp = dt_printf_create(pcb->pcb_hdl, fmt);
			dt_printf_validate(pfp, DT_PRINTF_AGGREGATION,
					   dnp->dn_ident, 1, fid->di_id,
					   ((dt_idsig_t *)aid->di_data)->dis_args);
		}

		dt_cg_store_val(pcb, anp, DTRACEACT_PRINTA, pfp, (uint64_t)dnp);
		pfp = NULL;

		anp = anp->dn_list;
		argr++;
	}
}

static void
dt_cg_act_printf(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind)
{
	dt_node_t	*arg1, *anp;
	dt_pfargv_t	*pfp;
	char		n[DT_TYPE_NAMELEN];
	char		*str;

	/*
	 * Ensure that the format string is a string constant.
	 */
	if (dnp->dn_args->dn_kind != DT_NODE_STRING) {
		dnerror(dnp, D_PRINTF_ARG_FMT,
			"%s( ) argument #1 is incompatible with prototype:\n"
			"\tprototype: string constant\n\t argument: %s\n",
			dnp->dn_ident->di_name,
			dt_node_type_name(dnp->dn_args, n, sizeof(n)));
	}

	arg1 = dnp->dn_args->dn_list;
	yylineno = dnp->dn_line;
	str = dnp->dn_args->dn_string;

	/*
	 * If this is an freopen(), we use an empty string to denote that
	 * stdout should be restored.  For other printf()-like actions, an
	 * empty format string is illegal:  an empty format string would
	 * result in malformed DOF, and the compiler thus flags an empty
	 * format string as a compile-time error.  To avoid propagating the
	 * freopen() special case throughout the system, we simply transpose
	 * an empty string into a sentinel string (DT_FREOPEN_RESTORE) that
	 * denotes that stdout should be restored.
	 */
	if (kind == DTRACEACT_FREOPEN) {
		if (strcmp(str, DT_FREOPEN_RESTORE) == 0) {
			/*
			 * Our sentinel is always an invalid argument to
			 * freopen(), but if it's been manually specified, we
			 * must fail now instead of when the freopen() is
			 * actually evaluated.
			 */
			dnerror(dnp, D_FREOPEN_INVALID,
				"%s( ) argument #1 cannot be \"%s\"\n",
				dnp->dn_ident->di_name, DT_FREOPEN_RESTORE);
		}

		if (str[0] == '\0')
			str = DT_FREOPEN_RESTORE;
	}

	/*
	 * Validate the format string and the datatypes of the arguments.
	 */
	pfp = dt_printf_create(pcb->pcb_hdl, str);
	dt_printf_validate(pfp, DT_PRINTF_EXACTLEN, dnp->dn_ident, 1,
			   DTRACEACT_AGGREGATION, arg1);

	/*
	 * If no arguments are provided we will be printing a string constant.
	 * We do not write any data to the output buffer but we do need to add
	 * a record descriptor to indicate that at this point in the output
	 * stream, a string must be printed.
	 *
	 * If there are arguments, we need to generate code to store their
	 * values.
	 */
	if (arg1 == NULL)
		dt_rec_add(pcb->pcb_hdl, dt_cg_fill_gap, kind, 0, 1, pfp, 0);
	else {
		/*
		 * We pass the printf format descriptor along with the first
		 * record, and set it to NULL for subsequent records.  It is
		 * only used when the first record is encountered.
		 */
		for (anp = arg1; anp != NULL; anp = anp->dn_list) {
			dt_cg_store_val(pcb, anp, kind, pfp, 0);
			pfp = NULL;
		}
	}
}

static void
dt_cg_act_raise(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind)
{
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	dt_regset_t	*drp = pcb->pcb_regs;

	TRACE_REGSET("raise(): Begin ");

	dt_cg_node(dnp->dn_args, &pcb->pcb_ir, drp);

	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	emit(dlp, BPF_MOV_REG(BPF_REG_1, dnp->dn_args->dn_reg));
	dt_regset_free(drp, dnp->dn_args->dn_reg);
	dt_regset_xalloc(drp, BPF_REG_0);
	emit(dlp, BPF_CALL_HELPER(BPF_FUNC_send_signal));
	dt_regset_free_args(drp);

	/*
	 * FIXME:
	 * The raise() action should report an ILLOP fault if the signal is not
	 * valid (helper returns -EINVAL).  The BPF helper may also return
	 * -EPERM, -EBUSY, -ESRCH, or -EAGAIN - DTrace silently ignores those.
	 * Future changes may add support to handle these other failures modes
	 * of the BPF helper.
	 */

	dt_regset_free(drp, BPF_REG_0);

	TRACE_REGSET("raise(): End   ");
}

static void
dt_cg_act_setopt(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind)
{
}

/*
 * Signal that subsequent tracing output in the current clause should be kept
 * back pending a commit() or discard() for the speculation with the given id.
 *
 * Stores:
 *	integer (4 bytes)		-- speculation ID
 */
static void
dt_cg_act_speculate(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind)
{
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	uint_t		off;

	dt_cg_node(dnp->dn_args, &pcb->pcb_ir, pcb->pcb_regs);

	off = dt_rec_add(pcb->pcb_hdl, dt_cg_fill_gap, DTRACEACT_SPECULATE,
			 sizeof(uint64_t), sizeof(uint64_t), NULL,
			 DT_ACT_SPECULATE);

	emit(dlp, BPF_STORE(BPF_DW, BPF_REG_9, off, BPF_REG_0)); /* FIXME */
}

static void
dt_cg_act_stack(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind)
{
	dtrace_hdl_t	*dtp = pcb->pcb_hdl;
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	dt_regset_t	*drp = pcb->pcb_regs;
	dt_node_t	*arg = dnp->dn_args;
	int		nframes = dtp->dt_options[DTRACEOPT_STACKFRAMES];
	int		skip = 0;
	uint_t		off;
	uint_t		lbl_valid = dt_irlist_label(dlp);

	if (nframes == DTRACEOPT_UNSET)
		nframes = _dtrace_stackframes;

	if (arg != NULL) {
		if (!dt_node_is_posconst(arg))
			dnerror(arg, D_STACK_SIZE, "stack( ) argument #1 must "
				"be a non-zero positive integer constant\n");

		nframes = arg->dn_value;
	}

	/*
	 * If more frames are requested than allowed, silently reduce nframes.
	 */
	if (nframes > dtp->dt_options[DTRACEOPT_MAXFRAMES])
		nframes = dtp->dt_options[DTRACEOPT_MAXFRAMES];

	/* Reserve space in the output buffer. */
	off = dt_rec_add(dtp, dt_cg_fill_gap, DTRACEACT_STACK,
			 sizeof(uint64_t) * nframes, sizeof(uint64_t),
			 NULL, nframes);

	/* Now call bpf_get_stack(ctx, buf, size, flags). */
	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_1, BPF_REG_FP, DT_STK_DCTX));
	emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_1, BPF_REG_1, DCTX_CTX));
	emit(dlp,  BPF_MOV_REG(BPF_REG_2, BPF_REG_9));
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, off));
	emit(dlp,  BPF_MOV_IMM(BPF_REG_3, sizeof(uint64_t) * nframes));
	emit(dlp,  BPF_MOV_IMM(BPF_REG_4, skip & BPF_F_SKIP_FIELD_MASK));
	dt_regset_xalloc(drp, BPF_REG_0);
	emit(dlp,  BPF_CALL_HELPER(BPF_FUNC_get_stack));
	dt_regset_free_args(drp);
	emit(dlp,  BPF_BRANCH_IMM(BPF_JSGE, BPF_REG_0, 0, lbl_valid));
	dt_cg_probe_error(pcb, -1, DTRACEFLT_BADSTACK, 0);
	emitl(dlp, lbl_valid,
		   BPF_NOP());
	dt_regset_free(drp, BPF_REG_0);
}

static void
dt_cg_act_stop(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind)
{
	dnerror(dnp, D_UNKNOWN, "stop() is not implemented (yet)\n");
	/* FIXME: Needs implementation */
}

static void
dt_cg_act_symmod(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind)
{
	dt_node_t	*arg = dnp->dn_args;

	assert(arg != NULL);
	dt_cg_store_val(pcb, arg, kind, NULL, 0);
}

static void
dt_cg_act_trace(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind)
{
	char		n[DT_TYPE_NAMELEN];
	dt_node_t	*arg = dnp->dn_args;
	int		type = 0;

	if (dt_node_is_void(arg))
		dnerror(arg, D_TRACE_VOID,
			"trace( ) may not be applied to a void expression\n");

	if (dt_node_is_dynamic(arg))
		dnerror(arg, D_TRACE_DYN,
			"trace( ) may not be applied to a dynamic "
			"expression\n");

	if (arg->dn_flags & DT_NF_REF)
		type = DT_NF_REF;
	else if (arg->dn_flags & DT_NF_SIGNED)
		type = DT_NF_SIGNED;

	if (dt_cg_store_val(pcb, arg, DTRACEACT_DIFEXPR, NULL, type) == -1)
		dnerror(arg, D_PROTO_ARG,
			"trace( ) argument #1 is incompatible with prototype:\n"
			"\tprototype: scalar or string\n\t argument: %s\n",
			dt_node_type_name(arg, n, sizeof(n)));
}

static void
dt_cg_act_tracemem(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind)
{
}

static void
dt_cg_act_trunc(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind)
{
	dt_node_t	*anp, *trunc;
	dt_ident_t	*aid;
	char		n[DT_TYPE_NAMELEN];
	int		argc = 0;

	for (anp = dnp->dn_args; anp != NULL; anp = anp->dn_list)
		argc++;
	assert(argc == 1 || argc == 2);

	anp = dnp->dn_args;
	assert(anp != NULL);
	if (anp->dn_kind != DT_NODE_AGG)
		dnerror(dnp, D_TRUNC_AGGARG,
			"%s( ) argument #1 is incompatible with prototype:\n"
			"\tprototype: aggregation\n\t argument: %s\n",
			dnp->dn_ident->di_name,
			dt_node_type_name(anp, n, sizeof(n)));

	trunc = anp->dn_list;
	if (argc == 2)
		assert(trunc != NULL && dt_node_is_scalar(trunc));

	aid = anp->dn_ident;
	if (aid->di_gen == pcb->pcb_hdl->dt_gen &&
	    !(aid->di_flags & DT_IDFLG_MOD))
		dnerror(dnp, D_TRUNC_AGGBAD,
			"undefined aggregation: @%s\n", aid->di_name);

	/*
	 * FIXME: Needs implementation
	 * TODO: Emit code to truncate the given aggregation.
	 * DEPENDS ON: How aggregations are implemented using eBPF (hashmap?).
	 * AGGID = aid->di_id
	 */
	dt_cg_store_val(pcb, anp, DTRACEACT_LIBACT, NULL, DT_ACT_TRUNC);
#ifdef FIXME
	/*
	 * FIXME: There is an optional trunction value.
	 * if (argc == 1), the optional value is missing, but "0" may be
	 * specified.
	 */
	dt_cg_store_val(pcb, trunc, DTRACEACT_LIBACT, NULL, DT_ACT_TRUNC);
#endif
}

static void
dt_cg_act_ustack(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind)
{
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	dt_regset_t	*drp = pcb->pcb_regs;
	dtrace_hdl_t	*dtp = pcb->pcb_hdl;
	int		nframes = dtp->dt_options[DTRACEOPT_USTACKFRAMES];
	int		strsize = 0;
	int		stacksize;
	dt_node_t	*arg0 = dnp->dn_args;
	dt_node_t	*arg1 = arg0 != NULL ? arg0->dn_list : NULL;
	int		skip = 0;
	uint_t		off;
	uint_t		lbl_valid = dt_irlist_label(dlp);

	if (nframes == DTRACEOPT_UNSET)
		nframes = _dtrace_ustackframes;

	if (arg0 != NULL) {
		if (!dt_node_is_posconst(arg0))
			dnerror(arg0, D_USTACK_FRAMES, "ustack( ) argument #1 "
				"must be a non-zero positive integer "
				"constant\n");

		nframes = arg0->dn_value;
	}

	if (nframes > dtp->dt_options[DTRACEOPT_MAXFRAMES])
		nframes = dtp->dt_options[DTRACEOPT_MAXFRAMES];

	/* FIXME: for now, accept non-zero strsize, but it does nothing */
	if (arg1 != NULL) {
		if (arg1->dn_kind != DT_NODE_INT ||
		    ((arg1->dn_flags & DT_NF_SIGNED) &&
		    (int64_t)arg1->dn_value < 0))
			dnerror(arg1, D_USTACK_STRSIZE, "ustack( ) argument #2 "
				"must be a positive integer constant\n");

		strsize = arg1->dn_value;
	}

	/* Reserve space in the output buffer. */
	stacksize = nframes * sizeof(uint64_t);
	off = dt_rec_add(pcb->pcb_hdl, dt_cg_fill_gap, DTRACEACT_USTACK,
			 8 + stacksize, 8, NULL,
			 DTRACE_USTACK_ARG(nframes, strsize));

	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	dt_regset_xalloc(drp, BPF_REG_0);

	/* Write the tgid. */
	emit(dlp,  BPF_CALL_HELPER(BPF_FUNC_get_current_pid_tgid));
	emit(dlp,  BPF_ALU64_IMM(BPF_AND, BPF_REG_0, 0xffffffff));
	emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_9, off, BPF_REG_0));

	/* Now call bpf_get_stack(ctx, buf, size, flags). */
	emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_1, BPF_REG_FP, DT_STK_DCTX));
	emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_1, BPF_REG_1, DCTX_CTX));
	emit(dlp,  BPF_MOV_REG(BPF_REG_2, BPF_REG_9));
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, off + 8));
	emit(dlp,  BPF_MOV_IMM(BPF_REG_3, stacksize));
	emit(dlp,  BPF_MOV_IMM(BPF_REG_4, (skip & BPF_F_SKIP_FIELD_MASK)
					  | BPF_F_USER_STACK));
	emit(dlp,  BPF_CALL_HELPER(BPF_FUNC_get_stack));
	dt_regset_free_args(drp);
	emit(dlp,  BPF_BRANCH_IMM(BPF_JSGE, BPF_REG_0, 0, lbl_valid));
	dt_cg_probe_error(pcb, -1, DTRACEFLT_BADSTACK, 0);
	emitl(dlp, lbl_valid,
		   BPF_NOP());
	dt_regset_free(drp, BPF_REG_0);
}

typedef void dt_cg_action_f(dt_pcb_t *, dt_node_t *, dtrace_actkind_t);

typedef struct dt_cg_actdesc {
	dt_cg_action_f *fun;
	dtrace_actkind_t kind;
} dt_cg_actdesc_t;

static const dt_cg_actdesc_t _dt_cg_actions[DT_ACT_MAX] = {
	[DT_ACT_IDX(DT_ACT_PRINTF)]		= { &dt_cg_act_printf,
						    DTRACEACT_PRINTF },
	[DT_ACT_IDX(DT_ACT_TRACE)]		= { &dt_cg_act_trace, },
	[DT_ACT_IDX(DT_ACT_TRACEMEM)]		= { &dt_cg_act_tracemem, },
	[DT_ACT_IDX(DT_ACT_STACK)]		= { &dt_cg_act_stack, },
	[DT_ACT_IDX(DT_ACT_STOP)]		= { &dt_cg_act_stop,
						    DTRACEACT_STOP },
	[DT_ACT_IDX(DT_ACT_BREAKPOINT)]		= { &dt_cg_act_breakpoint,
						    DTRACEACT_BREAKPOINT },
	[DT_ACT_IDX(DT_ACT_PANIC)]		= { &dt_cg_act_panic,
						    DTRACEACT_PANIC },
	[DT_ACT_IDX(DT_ACT_SPECULATE)]		= { &dt_cg_act_speculate,
						    DTRACEACT_SPECULATE },
	[DT_ACT_IDX(DT_ACT_COMMIT)]		= { &dt_cg_act_commit,
						    DTRACEACT_COMMIT },
	[DT_ACT_IDX(DT_ACT_DISCARD)]		= { &dt_cg_act_discard,
						    DTRACEACT_DISCARD },
	[DT_ACT_IDX(DT_ACT_CHILL)]		= { &dt_cg_act_chill,
						    DTRACEACT_CHILL },
	[DT_ACT_IDX(DT_ACT_EXIT)]		= { &dt_cg_act_exit,
						    DTRACEACT_EXIT },
	[DT_ACT_IDX(DT_ACT_USTACK)]		= { &dt_cg_act_ustack, },
	[DT_ACT_IDX(DT_ACT_PRINTA)]		= { &dt_cg_act_printa, },
	[DT_ACT_IDX(DT_ACT_RAISE)]		= { &dt_cg_act_raise,
						    DTRACEACT_RAISE },
	[DT_ACT_IDX(DT_ACT_CLEAR)]		= { &dt_cg_act_clear, },
	[DT_ACT_IDX(DT_ACT_NORMALIZE)]		= { &dt_cg_act_normalize, },
	[DT_ACT_IDX(DT_ACT_DENORMALIZE)]	= { &dt_cg_act_denormalize, },
	[DT_ACT_IDX(DT_ACT_TRUNC)]		= { &dt_cg_act_trunc, },
	[DT_ACT_IDX(DT_ACT_SYSTEM)]		= { &dt_cg_act_printf,
						    DTRACEACT_SYSTEM },
	[DT_ACT_IDX(DT_ACT_JSTACK)]		= { &dt_cg_act_jstack, },
	[DT_ACT_IDX(DT_ACT_FTRUNCATE)]		= { &dt_cg_act_ftruncate, },
	[DT_ACT_IDX(DT_ACT_FREOPEN)]		= { &dt_cg_act_printf,
						    DTRACEACT_FREOPEN },
	[DT_ACT_IDX(DT_ACT_SYM)]		= { &dt_cg_act_symmod,
						    DTRACEACT_SYM },
	[DT_ACT_IDX(DT_ACT_MOD)]		= { &dt_cg_act_symmod,
						    DTRACEACT_MOD },
	[DT_ACT_IDX(DT_ACT_USYM)]		= { &dt_cg_act_symmod,
						    DTRACEACT_USYM },
	[DT_ACT_IDX(DT_ACT_UMOD)]		= { &dt_cg_act_symmod,
						    DTRACEACT_UMOD },
	[DT_ACT_IDX(DT_ACT_UADDR)]		= { &dt_cg_act_symmod,
						    DTRACEACT_UADDR },
	[DT_ACT_IDX(DT_ACT_SETOPT)]		= { &dt_cg_act_setopt, },
	[DT_ACT_IDX(DT_ACT_PCAP)]		= { &dt_cg_act_pcap, },
};

dt_irnode_t *
dt_cg_node_alloc(uint_t label, struct bpf_insn instr)
{
	dt_irnode_t *dip = malloc(sizeof(dt_irnode_t));

	if (dip == NULL)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOMEM);

	dip->di_label = label;
	dip->di_instr = instr;
	dip->di_extern = NULL;
	dip->di_next = NULL;

	return dip;
}

/*
 * Code generator wrapper function for ctf_member_info.  If we are given a
 * reference to a forward declaration tag, search the entire type space for
 * the actual definition and then call ctf_member_info on the result.
 */
static ctf_file_t *
dt_cg_membinfo(ctf_file_t *fp, ctf_id_t type, const char *s, ctf_membinfo_t *mp)
{
	while (ctf_type_kind(fp, type) == CTF_K_FORWARD) {
		char n[DT_TYPE_NAMELEN];
		dtrace_typeinfo_t dtt;

		if (ctf_type_name(fp, type, n, sizeof(n)) == NULL ||
		    dt_type_lookup(n, &dtt) == -1 || (
		    dtt.dtt_ctfp == fp && dtt.dtt_type == type))
			break; /* unable to improve our position */

		fp = dtt.dtt_ctfp;
		type = ctf_type_resolve(fp, dtt.dtt_type);
	}

	if (ctf_member_info(fp, type, s, mp) == CTF_ERR)
		return NULL; /* ctf_errno is set for us */

	return fp;
}

void
dt_cg_xsetx(dt_irlist_t *dlp, dt_ident_t *idp, uint_t lbl, int reg, uint64_t x)
{
	struct bpf_insn instr[2] = { BPF_LDDW(reg, x) };

	emitle(dlp, lbl,
		    instr[0], idp);
	emit(dlp,   instr[1]);
}

static void
dt_cg_setx(dt_irlist_t *dlp, int reg, uint64_t x)
{
	dt_cg_xsetx(dlp, NULL, DT_LBL_NONE, reg, x);
}

/*
 * When loading bit-fields, we want to convert a byte count in the range
 * 1-8 to the closest power of 2 (e.g. 3->4, 5->8, etc).  The clp2() function
 * is a clever implementation from "Hacker's Delight" by Henry Warren, Jr.
 */
static size_t
clp2(size_t x)
{
	x--;

	x |= (x >> 1);
	x |= (x >> 2);
	x |= (x >> 4);
	x |= (x >> 8);
	x |= (x >> 16);

	return x + 1;
}

/*
 * Lookup the correct load opcode to use for the specified node and CTF type.
 * We determine the size and convert it to a 3-bit index.  Our lookup table
 * is constructed to use a 5-bit index, consisting of the 3-bit size 0-7, a
 * bit for the sign, and a bit for userland address.  For example, a 4-byte
 * signed load from userland would be at the following table index:
 * user=1 sign=1 size=4 => binary index 11011 = decimal index 27
 */
static uint_t
dt_cg_load(dt_node_t *dnp, ctf_file_t *ctfp, ctf_id_t type)
{
#if 1
	ctf_encoding_t e;
	ssize_t size;

	/*
	 * If we're loading a bit-field, the size of our load is found by
	 * rounding cte_bits up to a byte boundary and then finding the
	 * nearest power of two to this value (see clp2(), above).
	 */
	if ((dnp->dn_flags & DT_NF_BITFIELD) &&
	    ctf_type_encoding(ctfp, type, &e) != CTF_ERR)
		size = clp2(P2ROUNDUP(e.cte_bits, NBBY) / NBBY);
	else
		size = ctf_type_size(ctfp, type);

	if (size < 1 || size > 8 || (size & (size - 1)) != 0) {
		xyerror(D_UNKNOWN, "internal error -- cg cannot load "
		    "size %ld when passed by value\n", (long)size);
	}

	return ldstw[size];
#else
	static const uint_t ops[] = {
		DIF_OP_LDUB,	DIF_OP_LDUH,	0,	DIF_OP_LDUW,
		0,		0,		0,	DIF_OP_LDX,
		DIF_OP_LDSB,	DIF_OP_LDSH,	0,	DIF_OP_LDSW,
		0,		0,		0,	DIF_OP_LDX,
		DIF_OP_ULDUB,	DIF_OP_ULDUH,	0,	DIF_OP_ULDUW,
		0,		0,		0,	DIF_OP_ULDX,
		DIF_OP_ULDSB,	DIF_OP_ULDSH,	0,	DIF_OP_ULDSW,
		0,		0,		0,	DIF_OP_ULDX,
	};

	ctf_encoding_t e;
	ssize_t size;

	/*
	 * If we're loading a bit-field, the size of our load is found by
	 * rounding cte_bits up to a byte boundary and then finding the
	 * nearest power of two to this value (see clp2(), above).
	 */
	if ((dnp->dn_flags & DT_NF_BITFIELD) &&
	    ctf_type_encoding(ctfp, type, &e) != CTF_ERR)
		size = clp2(P2ROUNDUP(e.cte_bits, NBBY) / NBBY);
	else
		size = ctf_type_size(ctfp, type);

	if (size < 1 || size > 8 || (size & (size - 1)) != 0) {
		xyerror(D_UNKNOWN, "internal error -- cg cannot load "
		    "size %ld when passed by value\n", (long)size);
	}

	size--; /* convert size to 3-bit index */

	if (dnp->dn_flags & DT_NF_SIGNED)
		size |= 0x08;
	if (dnp->dn_flags & DT_NF_USERLAND)
		size |= 0x10;

	return ops[size];
#endif
}

static void
dt_cg_load_var(dt_node_t *dst, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_ident_t	*idp = dt_ident_resolve(dst->dn_ident);

	idp->di_flags |= DT_IDFLG_DIFR;

	/* global and local variables (not thread-local or built-in) */
	if ((idp->di_flags & DT_IDFLG_LOCAL) ||
	    (!(idp->di_flags & DT_IDFLG_TLS) &&
	     idp->di_id >= DIF_VAR_OTHER_UBASE)) {
		if ((dst->dn_reg = dt_regset_alloc(drp)) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

		/* get pointer to BPF map */
		emit(dlp, BPF_LOAD(BPF_DW, dst->dn_reg, BPF_REG_FP, DT_STK_DCTX));
		if (idp->di_flags & DT_IDFLG_LOCAL)
			emit(dlp, BPF_LOAD(BPF_DW, dst->dn_reg, dst->dn_reg, DCTX_LVARS));
		else
			emit(dlp, BPF_LOAD(BPF_DW, dst->dn_reg, dst->dn_reg, DCTX_GVARS));

		/* load the variable value or address */
		if (dst->dn_flags & DT_NF_REF)
			emit(dlp, BPF_ALU64_IMM(BPF_ADD, dst->dn_reg, idp->di_offset));
		else {
			size_t	size = dt_node_type_size(dst);

			assert(size > 0 && size <= 8 &&
			       (size & (size - 1)) == 0);

			emit(dlp, BPF_LOAD(ldstw[size], dst->dn_reg, dst->dn_reg, idp->di_offset));
		}

		return;
	}

	/* otherwise, handle thread-local and built-in variables */
	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	if (idp->di_flags & DT_IDFLG_TLS) {	/* TLS var */
		emit(dlp, BPF_MOV_IMM(BPF_REG_1, idp->di_id - DIF_VAR_OTHER_UBASE));
		idp = dt_dlib_get_func(yypcb->pcb_hdl, "dt_get_tvar");
	} else if (idp->di_id < DIF_VAR_OTHER_UBASE) {	/* built-in var */
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_1, BPF_REG_FP, DT_STK_DCTX));
		emit(dlp, BPF_MOV_IMM(BPF_REG_2, idp->di_id));
		idp = dt_dlib_get_func(yypcb->pcb_hdl, "dt_get_bvar");
	} else
		assert(0);
	assert(idp != NULL);
	dt_regset_xalloc(drp, BPF_REG_0);
	emite(dlp, BPF_CALL_FUNC(idp->di_id), idp);
	dt_regset_free_args(drp);

	dt_cg_check_fault(yypcb);

	if ((dst->dn_reg = dt_regset_alloc(drp)) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	emit(dlp,  BPF_MOV_REG(dst->dn_reg, BPF_REG_0));
	dt_regset_free(drp, BPF_REG_0);
}

static void
dt_cg_ptrsize(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp,
    uint_t op, int dreg)
{
	ctf_file_t *ctfp = dnp->dn_ctfp;
	ctf_arinfo_t r;
	ctf_id_t type;
	uint_t kind;
	ssize_t size;

	type = ctf_type_resolve(ctfp, dnp->dn_type);
	kind = ctf_type_kind(ctfp, type);
	assert(kind == CTF_K_POINTER || kind == CTF_K_ARRAY);

	if (kind == CTF_K_ARRAY) {
		if (ctf_array_info(ctfp, type, &r) != 0) {
			yypcb->pcb_hdl->dt_ctferr = ctf_errno(ctfp);
			longjmp(yypcb->pcb_jmpbuf, EDT_CTF);
		}
		type = r.ctr_contents;
	} else
		type = ctf_type_reference(ctfp, type);

	if ((size = ctf_type_size(ctfp, type)) == 1)
		return; /* multiply or divide by one can be omitted */

	emit(dlp, BPF_ALU64_IMM(op, dreg, size));
}

/*
 * If the result of a "." or "->" operation is a bit-field, we use this routine
 * to generate an epilogue to the load instruction that extracts the value.
 */
static void
dt_cg_field_get(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp,
    ctf_file_t *fp, const ctf_membinfo_t *mp)
{
	ctf_encoding_t e;
	uint64_t shift;
	int r1;

	if (ctf_type_encoding(fp, mp->ctm_type, &e) != 0 || e.cte_bits > 64) {
		xyerror(D_UNKNOWN, "cg: bad field: off %lu type <%ld> "
		    "bits %u\n", mp->ctm_offset, mp->ctm_type, e.cte_bits);
	}

	assert(dnp->dn_op == DT_TOK_PTR || dnp->dn_op == DT_TOK_DOT);
	r1 = dnp->dn_left->dn_reg;

	/*
	 * On little-endian architectures, ctm_offset counts from the right so
	 * ctm_offset % NBBY itself is the amount we want to shift right to
	 * move the value bits to the little end of the register to mask them.
	 * On big-endian architectures, ctm_offset counts from the left so we
	 * must subtract (ctm_offset % NBBY + cte_bits) from the size in bits
	 * we used for the load.  The size of our load in turn is found by
	 * rounding cte_bits up to a byte boundary and then finding the
	 * nearest power of two to this value (see clp2(), above).
	 */
	if (dnp->dn_flags & DT_NF_SIGNED) {
		/*
		 * r1 <<= 64 - shift
		 * r1 >>= 64 - bits
		 */
#ifdef _BIG_ENDIAN
		shift = clp2(P2ROUNDUP(e.cte_bits, NBBY) / NBBY) * NBBY -
		    mp->ctm_offset % NBBY;
#else
		shift = mp->ctm_offset % NBBY + e.cte_bits;
#endif
		emit(dlp, BPF_ALU64_IMM(BPF_LSH, r1, 64 - shift));
		emit(dlp, BPF_ALU64_IMM(BPF_ARSH, r1, 64 - e.cte_bits));
	} else {
		/*
		 * r1 >>= shift
		 * r1 &= (1 << bits) - 1
		 */
#ifdef _BIG_ENDIAN
		shift = clp2(P2ROUNDUP(e.cte_bits, NBBY) / NBBY) * NBBY -
		    (mp->ctm_offset % NBBY + e.cte_bits);
#else
		shift = mp->ctm_offset % NBBY;
#endif
		emit(dlp, BPF_ALU64_IMM(BPF_RSH, r1, shift));
		emit(dlp, BPF_ALU64_IMM(BPF_AND, r1, (1ULL << e.cte_bits) - 1));
	}
}

/*
 * If the destination of a store operation is a bit-field, we first use this
 * routine to load the surrounding bits, clear the destination field, and OR
 * in the new value of the field.  After this routine, we will store the
 * generated word.
 *
 * This routine allocates a new register to hold the value to be stored and
 * returns it.  The caller is responsible for freeing this register later.
 */
static int
dt_cg_field_set(dt_node_t *src, dt_irlist_t *dlp,
    dt_regset_t *drp, dt_node_t *dst)
{
	uint64_t cmask, fmask, shift;
	int r1, r2;

	ctf_membinfo_t m;
	ctf_encoding_t e;
	ctf_file_t *fp, *ofp;
	ctf_id_t type;

	assert(dst->dn_op == DT_TOK_PTR || dst->dn_op == DT_TOK_DOT);
	assert(dst->dn_right->dn_kind == DT_NODE_IDENT);

	fp = dst->dn_left->dn_ctfp;
	type = ctf_type_resolve(fp, dst->dn_left->dn_type);

	if (dst->dn_op == DT_TOK_PTR) {
		type = ctf_type_reference(fp, type);
		type = ctf_type_resolve(fp, type);
	}

	if ((fp = dt_cg_membinfo(ofp = fp, type,
	    dst->dn_right->dn_string, &m)) == NULL) {
		yypcb->pcb_hdl->dt_ctferr = ctf_errno(ofp);
		longjmp(yypcb->pcb_jmpbuf, EDT_CTF);
	}

	if (ctf_type_encoding(fp, m.ctm_type, &e) != 0 || e.cte_bits > 64) {
		xyerror(D_UNKNOWN, "cg: bad field: off %lu type <%ld> "
		    "bits %u\n", m.ctm_offset, m.ctm_type, e.cte_bits);
	}

	if ((r1 = dt_regset_alloc(drp)) == -1 ||
	    (r2 = dt_regset_alloc(drp)) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	/*
	 * Compute shifts and masks.  We need to compute "shift" as the amount
	 * we need to shift left to position our field in the containing word.
	 * Refer to the comments in dt_cg_field_get(), above, for more info.
	 * We then compute fmask as the mask that truncates the value in the
	 * input register to width cte_bits, and cmask as the mask used to
	 * pass through the containing bits and zero the field bits.
	 */
#ifdef _BIG_ENDIAN
	shift = clp2(P2ROUNDUP(e.cte_bits, NBBY) / NBBY) * NBBY -
	    (m.ctm_offset % NBBY + e.cte_bits);
#else
	shift = m.ctm_offset % NBBY;
#endif
	fmask = (1ULL << e.cte_bits) - 1;
	cmask = ~(fmask << shift);

	/*
	 * r1 = [dst->dn_reg]
	 * r2 = cmask
	 * r1 &= r2
	 * r2 = fmask
	 * r2 &= src->dn_reg
	 * r2 <<= shift
	 * r1 |= r2
	 */
	/* FIXME: Does not handle userland */
	emit(dlp, BPF_LOAD(dt_cg_load(dst, fp, m.ctm_type), r1, dst->dn_reg, 0));
	dt_cg_setx(dlp, r2, cmask);
	emit(dlp, BPF_ALU64_REG(BPF_AND, r1, r2));
	dt_cg_setx(dlp, r2, fmask);
	emit(dlp, BPF_ALU64_REG(BPF_AND, r2, src->dn_reg));
	emit(dlp, BPF_ALU64_IMM(BPF_LSH, r2, shift));
	emit(dlp, BPF_ALU64_REG(BPF_OR, r1, r2));
	dt_regset_free(drp, r2);

	return r1;
}

static void
dt_cg_store(dt_node_t *src, dt_irlist_t *dlp, dt_regset_t *drp, dt_node_t *dst)
{
	ctf_encoding_t e;
	size_t size;
	int reg;

	/*
	 * If we're loading a bit-field, the size of our store is found by
	 * rounding dst's cte_bits up to a byte boundary and then finding the
	 * nearest power of two to this value (see clp2(), above).
	 */
	if ((dst->dn_flags & DT_NF_BITFIELD) &&
	    ctf_type_encoding(dst->dn_ctfp, dst->dn_type, &e) != CTF_ERR)
		size = clp2(P2ROUNDUP(e.cte_bits, NBBY) / NBBY);
	else
		size = dt_node_type_size(dst);

	if (src->dn_flags & DT_NF_REF)
		dt_cg_memcpy(dlp, drp, dst->dn_reg, src->dn_reg, size);
	else {
		if (dst->dn_flags & DT_NF_BITFIELD)
			reg = dt_cg_field_set(src, dlp, drp, dst);
		else
			reg = src->dn_reg;

		assert(size > 0 && size <= 8 && (size & (size - 1)) == 0);

		emit(dlp, BPF_STORE(ldstw[size], dst->dn_reg, 0, reg));

		if (dst->dn_flags & DT_NF_BITFIELD)
			dt_regset_free(drp, reg);
	}
}

static void
dt_cg_store_var(dt_node_t *src, dt_irlist_t *dlp, dt_regset_t *drp,
		dt_ident_t *idp)
{
	uint_t	varid;

	idp->di_flags |= DT_IDFLG_DIFW;

	/* global and local variables (that is, not thread-local) */
	if (!(idp->di_flags & DT_IDFLG_TLS)) {
		int reg;

		if ((reg = dt_regset_alloc(drp)) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

		/* get pointer to BPF map */
		emit(dlp, BPF_LOAD(BPF_DW, reg, BPF_REG_FP, DT_STK_DCTX));
		if (idp->di_flags & DT_IDFLG_LOCAL)
			emit(dlp, BPF_LOAD(BPF_DW, reg, reg, DCTX_LVARS));
		else
			emit(dlp, BPF_LOAD(BPF_DW, reg, reg, DCTX_GVARS));

		/* store by value or by reference */
		if (src->dn_flags & DT_NF_REF) {
			size_t	size;

			emit(dlp, BPF_ALU64_IMM(BPF_ADD, reg, idp->di_offset));

			/*
			 * Determine the amount of data to be copied.  It is
			 * usually the size of the identifier, except for
			 * string constants where it is the size of the string
			 * constant (adjusted for the variable-width length
			 * prefix).  An assignment of a string constant is a
			 * store of type 'string' with a RHS of type
			 * DT_TOK_STRING.
			 */
			if (dt_node_is_string(src) &&
			    src->dn_right->dn_op == DT_TOK_STRING) {
				size = dt_node_type_size(src->dn_right);
				size += dt_vint_size(size);
			} else
				size = idp->di_size;

			dt_cg_memcpy(dlp, drp, reg, src->dn_reg, size);
		} else {
			size_t	size = idp->di_size;

			assert(size > 0 && size <= 8 &&
			       (size & (size - 1)) == 0);

			emit(dlp, BPF_STORE(ldstw[size], reg, idp->di_offset, src->dn_reg));
		}

		dt_regset_free(drp, reg);
		return;
	}

	/* TLS var */
	varid = idp->di_id - DIF_VAR_OTHER_UBASE;
	idp = dt_dlib_get_func(yypcb->pcb_hdl, "dt_set_tvar");
	assert(idp != NULL);

	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	/*
	 * We assign the varid in the instruction preceding the call because
	 * the disassembler expects this sequence in support for annotating
	 * the disassembly with variables names.
	 */
	emit(dlp,  BPF_MOV_REG(BPF_REG_2, src->dn_reg));
	emit(dlp,  BPF_MOV_IMM(BPF_REG_1, varid));
	dt_regset_xalloc(drp, BPF_REG_0);
	emite(dlp, BPF_CALL_FUNC(idp->di_id), idp);
	dt_regset_free(drp, BPF_REG_0);
	dt_regset_free_args(drp);
}

/*
 * Generate code for a typecast or for argument promotion from the type of the
 * actual to the type of the formal.  We need to generate code for casts when
 * a scalar type is being narrowed or changing signed-ness.  We first shift the
 * desired bits high (losing excess bits if narrowing) and then shift them down
 * using logical shift (unsigned result) or arithmetic shift (signed result).
 */
static void
dt_cg_typecast(const dt_node_t *src, const dt_node_t *dst,
    dt_irlist_t *dlp, dt_regset_t *drp)
{
	size_t srcsize;
	size_t dstsize;
	int n;

	/* If the destination type is '@' (any type) we need not cast. */
	if (dst->dn_ctfp == NULL && dst->dn_type == CTF_ERR)
		return;

	srcsize = dt_node_type_size(src);
	dstsize = dt_node_type_size(dst);

	if (dstsize < srcsize)
		n = sizeof(uint64_t) * NBBY - dstsize * NBBY;
	else
		n = sizeof(uint64_t) * NBBY - srcsize * NBBY;

	if (dt_node_is_scalar(dst) && n != 0 && (dstsize < srcsize ||
	    (src->dn_flags & DT_NF_SIGNED) ^ (dst->dn_flags & DT_NF_SIGNED))) {
		emit(dlp, BPF_MOV_REG(dst->dn_reg, src->dn_reg));
		emit(dlp, BPF_ALU64_IMM(BPF_LSH, dst->dn_reg, n));
		emit(dlp, BPF_ALU64_IMM((dst->dn_flags & DT_NF_SIGNED) ? BPF_ARSH : BPF_RSH, dst->dn_reg, n));
	}
}

/*
 * Generate code to push the specified argument list on to the tuple stack.
 * We use this routine for handling subroutine calls and associative arrays.
 * We must first generate code for all subexpressions before loading the stack
 * because any subexpression could itself require the use of the tuple stack.
 * This holds a number of registers equal to the number of arguments, but this
 * is not a huge problem because the number of arguments can't exceed the
 * number of tuple register stack elements anyway.  At most one extra register
 * is required (either by dt_cg_typecast() or for dtdt_size, below).  This
 * implies that a DIF implementation should offer a number of general purpose
 * registers at least one greater than the number of tuple registers.
 */
static void
dt_cg_arglist(dt_ident_t *idp, dt_node_t *args,
    dt_irlist_t *dlp, dt_regset_t *drp)
{
	const dt_idsig_t *isp = idp->di_data;
	dt_node_t *dnp;
	int i = 0;

	for (dnp = args; dnp != NULL; dnp = dnp->dn_list)
		dt_cg_node(dnp, dlp, drp);

	for (dnp = args; dnp != NULL; dnp = dnp->dn_list) {
		dtrace_diftype_t t;
		uint_t op;
		int reg;

		dt_node_diftype(yypcb->pcb_hdl, dnp, &t);

		isp->dis_args[i].dn_reg = dnp->dn_reg; /* re-use register */
		dt_cg_typecast(dnp, &isp->dis_args[i], dlp, drp);
		isp->dis_args[i].dn_reg = -1;

		if (t.dtdt_flags & DIF_TF_BYREF)
			op = DIF_OP_PUSHTR;
		else
			op = DIF_OP_PUSHTV;

		if (t.dtdt_size != 0) {
			if ((reg = dt_regset_alloc(drp)) == -1)
				longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
			dt_cg_setx(dlp, reg, t.dtdt_size);
		} else
			reg = DIF_REG_R0;

#if 0
		instr = DIF_INSTR_PUSHTS(op, t.dtdt_kind, reg, dnp->dn_reg);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
#else
		emit(dlp, BPF_CALL_FUNC(op));
#endif
		dt_regset_free(drp, dnp->dn_reg);

		if (reg != DIF_REG_R0)
			dt_regset_free(drp, reg);
		if (i < isp->dis_varargs)
			i++;
	}

	if (i > yypcb->pcb_hdl->dt_conf.dtc_diftupregs)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOTUPREG);
}

static void
dt_cg_arithmetic_op(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp,
		    uint_t op)
{
	int is_ptr_op = (dnp->dn_op == DT_TOK_ADD || dnp->dn_op == DT_TOK_SUB ||
	    dnp->dn_op == DT_TOK_ADD_EQ || dnp->dn_op == DT_TOK_SUB_EQ);

	int lp_is_ptr = dt_node_is_pointer(dnp->dn_left);
	int rp_is_ptr = dt_node_is_pointer(dnp->dn_right);

	if (lp_is_ptr && rp_is_ptr) {
		assert(dnp->dn_op == DT_TOK_SUB);
		is_ptr_op = 0;
	}

	dt_cg_node(dnp->dn_left, dlp, drp);
	if (is_ptr_op && rp_is_ptr)
		dt_cg_ptrsize(dnp, dlp, drp, BPF_MUL, dnp->dn_left->dn_reg);

	dt_cg_node(dnp->dn_right, dlp, drp);
	if (is_ptr_op && lp_is_ptr)
		dt_cg_ptrsize(dnp, dlp, drp, BPF_MUL, dnp->dn_right->dn_reg);

	if ((op == BPF_MOD || op == BPF_DIV) &&
	    (dnp->dn_flags & DT_NF_SIGNED)) {
		/*
		 * BPF_DIV and BPF_MOD are only for unsigned arguments.
		 * For signed arguments, use the following algorithm.
		 * The four right columns indicate the flow of steps.
		 *
		 *                   l > 0   l > 0   l < 0   l < 0
		 *                   r > 0   r < 0   r > 0   r < 0
		 *     jslt %rl,0,L3   1       1       1       1
		 *     jslt %rr,0,L1   2       2
		 *     ja   L4         3
		 * L1: neg  %rr                3
		 * L2: <op> %rl,%rr            4       4
		 *     neg  %rl                5       5
		 *     ja   L5                 6       6
		 * L3: neg  %rl                        2       2
		 *     jsge %rr,0,L2                   3       3
		 *     neg  %rr                                4
		 * L4: <op> %rl,%rr    4                       5
		 * L5: nop
		 */
		uint_t	lbl_L1 = dt_irlist_label(dlp);
		uint_t	lbl_L2 = dt_irlist_label(dlp);
		uint_t	lbl_L3 = dt_irlist_label(dlp);
		uint_t	lbl_L4 = dt_irlist_label(dlp);
		uint_t	lbl_L5 = dt_irlist_label(dlp);
		uint_t	lbl_valid = dt_irlist_label(dlp);

		/* First ensure we do not perform a division by zero. */
		emit(dlp,  BPF_BRANCH_IMM(BPF_JNE, dnp->dn_right->dn_reg, 0,
					  lbl_valid));
		dt_cg_probe_error(yypcb, -1, DTRACEFLT_DIVZERO, 0);
		emitl(dlp, lbl_valid,
			   BPF_NOP());

		emit(dlp,  BPF_BRANCH_IMM(BPF_JSLT, dnp->dn_left->dn_reg, 0, lbl_L3));
		emit(dlp,  BPF_BRANCH_IMM(BPF_JSLT, dnp->dn_right->dn_reg, 0, lbl_L1));
		emit(dlp,  BPF_JUMP(lbl_L4));

		emitl(dlp, lbl_L1,
			   BPF_NEG_REG(dnp->dn_right->dn_reg));

		emitl(dlp, lbl_L2,
			   BPF_ALU64_REG(op, dnp->dn_left->dn_reg, dnp->dn_right->dn_reg));
		emit(dlp,  BPF_NEG_REG(dnp->dn_left->dn_reg));
		emit(dlp,  BPF_JUMP(lbl_L5));

		emitl(dlp, lbl_L3,
			   BPF_NEG_REG(dnp->dn_left->dn_reg));
		emit(dlp,  BPF_BRANCH_IMM(BPF_JSGE, dnp->dn_right->dn_reg, 0, lbl_L2));
		emit(dlp,  BPF_NEG_REG(dnp->dn_right->dn_reg));

		emitl(dlp, lbl_L4,
			   BPF_ALU64_REG(op, dnp->dn_left->dn_reg, dnp->dn_right->dn_reg));

		emitl(dlp, lbl_L5,
			   BPF_NOP());
	} else
		emit(dlp,  BPF_ALU64_REG(op, dnp->dn_left->dn_reg, dnp->dn_right->dn_reg));

	dt_regset_free(drp, dnp->dn_right->dn_reg);
	dnp->dn_reg = dnp->dn_left->dn_reg;

	if (lp_is_ptr && rp_is_ptr)
		dt_cg_ptrsize(dnp->dn_right, dlp, drp, BPF_DIV, dnp->dn_reg);
}

#ifdef FIXME
static uint_t
dt_cg_stvar(const dt_ident_t *idp)
{
	static const uint_t aops[] = { DIF_OP_STGAA, DIF_OP_STTAA, DIF_OP_NOP };
	static const uint_t sops[] = { DIF_OP_STGS, DIF_OP_STTS, DIF_OP_STLS };

	uint_t i = (((idp->di_flags & DT_IDFLG_LOCAL) != 0) << 1) |
	    ((idp->di_flags & DT_IDFLG_TLS) != 0);

	return idp->di_kind == DT_IDENT_ARRAY ? aops[i] : sops[i];
}
#endif

static void
dt_cg_prearith_op(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp, uint_t op)
{
	ctf_file_t *ctfp = dnp->dn_ctfp;
	ctf_id_t type;
	ssize_t size = 1;

	if (dt_node_is_pointer(dnp)) {
		type = ctf_type_resolve(ctfp, dnp->dn_type);
		assert(ctf_type_kind(ctfp, type) == CTF_K_POINTER);
		size = ctf_type_size(ctfp, ctf_type_reference(ctfp, type));
	}

	dt_cg_node(dnp->dn_child, dlp, drp);
	dnp->dn_reg = dnp->dn_child->dn_reg;

	emit(dlp, BPF_ALU64_IMM(op, dnp->dn_reg, size));

	/*
	 * If we are modifying a variable, generate a store instruction for
	 * the variable specified by the identifier.  If we are storing to a
	 * memory address, generate code again for the left-hand side using
	 * DT_NF_REF to get the address, and then generate a store to it.
	 * In both paths, we store the value in dnp->dn_reg (the new value).
	 */
	if (dnp->dn_child->dn_kind == DT_NODE_VAR) {
		dt_ident_t *idp = dt_ident_resolve(dnp->dn_child->dn_ident);

		dt_cg_store_var(dnp, dlp, drp, idp);
	} else {
		uint_t rbit = dnp->dn_child->dn_flags & DT_NF_REF;

		assert(dnp->dn_child->dn_flags & DT_NF_WRITABLE);
		assert(dnp->dn_child->dn_flags & DT_NF_LVALUE);

		dnp->dn_child->dn_flags |= DT_NF_REF; /* force pass-by-ref */
		dt_cg_node(dnp->dn_child, dlp, drp);

		dt_cg_store(dnp, dlp, drp, dnp->dn_child);
		dt_regset_free(drp, dnp->dn_child->dn_reg);

		dnp->dn_left->dn_flags &= ~DT_NF_REF;
		dnp->dn_left->dn_flags |= rbit;
	}
}

static void
dt_cg_postarith_op(dt_node_t *dnp, dt_irlist_t *dlp,
    dt_regset_t *drp, uint_t op)
{
	ctf_file_t *ctfp = dnp->dn_ctfp;
	ctf_id_t type;
	ssize_t size = 1;
	int oreg, nreg;

	if (dt_node_is_pointer(dnp)) {
		type = ctf_type_resolve(ctfp, dnp->dn_type);
		assert(ctf_type_kind(ctfp, type) == CTF_K_POINTER);
		size = ctf_type_size(ctfp, ctf_type_reference(ctfp, type));
	}

	dt_cg_node(dnp->dn_child, dlp, drp);
	dnp->dn_reg = dnp->dn_child->dn_reg;
	oreg = dnp->dn_reg;

	nreg = dt_regset_alloc(drp);
	if (nreg == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	emit(dlp, BPF_MOV_REG(nreg, oreg));
	emit(dlp, BPF_ALU64_IMM(op, nreg, size));

	/*
	 * If we are modifying a variable, generate a store instruction for
	 * the variable specified by the identifier.  If we are storing to a
	 * memory address, generate code again for the left-hand side using
	 * DT_NF_REF to get the address, and then generate a store to it.
	 * In both paths, we store the value from %r0 (the new value).
	 */
	if (dnp->dn_child->dn_kind == DT_NODE_VAR) {
		dt_ident_t *idp = dt_ident_resolve(dnp->dn_child->dn_ident);

		dnp->dn_reg = nreg;
		dt_cg_store_var(dnp, dlp, drp, idp);
		dnp->dn_reg = oreg;
	} else {
		uint_t rbit = dnp->dn_child->dn_flags & DT_NF_REF;

		assert(dnp->dn_child->dn_flags & DT_NF_WRITABLE);
		assert(dnp->dn_child->dn_flags & DT_NF_LVALUE);

		dnp->dn_child->dn_flags |= DT_NF_REF; /* force pass-by-ref */
		dt_cg_node(dnp->dn_child, dlp, drp);

		dnp->dn_reg = nreg;
		dt_cg_store(dnp, dlp, drp, dnp->dn_child);
		dnp->dn_reg = oreg;

		dt_regset_free(drp, dnp->dn_child->dn_reg);
		dnp->dn_left->dn_flags &= ~DT_NF_REF;
		dnp->dn_left->dn_flags |= rbit;
	}

	dt_regset_free(drp, nreg);
}

/*
 * Determine if we should perform signed or unsigned comparison for an OP2.
 * If both operands are of arithmetic type, perform the usual arithmetic
 * conversions to determine the common real type for comparison [ISOC 6.5.8.3].
 */
static int
dt_cg_compare_signed(dt_node_t *dnp)
{
	dt_node_t dn;

	if (dt_node_is_string(dnp->dn_left) ||
	    dt_node_is_string(dnp->dn_right))
		return 1; /* strings always compare signed */
	else if (!dt_node_is_arith(dnp->dn_left) ||
	    !dt_node_is_arith(dnp->dn_right))
		return 0; /* non-arithmetic types always compare unsigned */

	memset(&dn, 0, sizeof(dn));
	dt_node_promote(dnp->dn_left, dnp->dn_right, &dn);
	return dn.dn_flags & DT_NF_SIGNED;
}

static void
dt_cg_compare_op(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp, uint_t op)
{
	uint_t lbl_true = dt_irlist_label(dlp);
	uint_t lbl_post = dt_irlist_label(dlp);

	dt_cg_node(dnp->dn_left, dlp, drp);
	dt_cg_node(dnp->dn_right, dlp, drp);

	/* FIXME: No support for string comparison yet */
	if (dt_node_is_string(dnp->dn_left) || dt_node_is_string(dnp->dn_right))
		xyerror(D_UNKNOWN, "internal error -- no support for "
			"string comparison yet\n");

	emit(dlp,  BPF_BRANCH_REG(op, dnp->dn_left->dn_reg, dnp->dn_right->dn_reg, lbl_true));
	dt_regset_free(drp, dnp->dn_right->dn_reg);
	dnp->dn_reg = dnp->dn_left->dn_reg;

	emit(dlp,  BPF_MOV_IMM(dnp->dn_reg, 0));
	emit(dlp,  BPF_JUMP(lbl_post));

	emitl(dlp, lbl_true,
		   BPF_MOV_IMM(dnp->dn_reg, 1));

	emitl(dlp, lbl_post,
		   BPF_NOP());
}

/*
 * Code generation for the ternary op requires some trickery with the assembler
 * in order to conserve registers.  We generate code for dn_expr and dn_left
 * and free their registers so they do not have be consumed across codegen for
 * dn_right.  We insert a dummy MOV at the end of dn_left into the destination
 * register, which is not yet known because we haven't done dn_right yet, and
 * save the pointer to this instruction node.  We then generate code for
 * dn_right and use its register as our output.  Finally, we reach back and
 * patch the instruction for dn_left to move its output into this register.
 */
static void
dt_cg_ternary_op(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	uint_t lbl_false = dt_irlist_label(dlp);
	uint_t lbl_post = dt_irlist_label(dlp);
	dt_irnode_t *dip;

	dt_cg_node(dnp->dn_expr, dlp, drp);
	emit(dlp, BPF_BRANCH_IMM(BPF_JEQ, dnp->dn_expr->dn_reg, 0, lbl_false));
	dt_regset_free(drp, dnp->dn_expr->dn_reg);

	dt_cg_node(dnp->dn_left, dlp, drp);
	/* save dip so we can patch it below */
	dip =
	emit(dlp,  BPF_MOV_IMM(dnp->dn_left->dn_reg, 0));
	dt_regset_free(drp, dnp->dn_left->dn_reg);
	emit(dlp,  BPF_JUMP(lbl_post));

	emitl(dlp, lbl_false,
		   BPF_NOP());

	dt_cg_node(dnp->dn_right, dlp, drp);
	dnp->dn_reg = dnp->dn_right->dn_reg;

	/*
	 * Now that dn_reg is assigned, reach back and patch the correct MOV
	 * instruction into the tail of dn_left.  We know dn_reg was unused
	 * at that point because otherwise dn_right couldn't have allocated it.
	 */
	dip->di_instr = BPF_MOV_REG(dnp->dn_reg, dnp->dn_left->dn_reg);

	emitl(dlp, lbl_post,
		   BPF_NOP());
}

static void
dt_cg_logical_and(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	uint_t lbl_false = dt_irlist_label(dlp);
	uint_t lbl_post = dt_irlist_label(dlp);

	dt_cg_node(dnp->dn_left, dlp, drp);
	emit(dlp, BPF_BRANCH_IMM(BPF_JEQ, dnp->dn_left->dn_reg, 0, lbl_false));
	dt_regset_free(drp, dnp->dn_left->dn_reg);

	dt_cg_node(dnp->dn_right, dlp, drp);
	emit(dlp,  BPF_BRANCH_IMM(BPF_JEQ, dnp->dn_right->dn_reg, 0, lbl_false));
	dnp->dn_reg = dnp->dn_right->dn_reg;

	dt_cg_setx(dlp, dnp->dn_reg, 1);
	emit(dlp,  BPF_JUMP(lbl_post));

	emitl(dlp, lbl_false,
		   BPF_MOV_IMM(dnp->dn_reg, 0));

	emitl(dlp, lbl_post,
		   BPF_NOP());
}

static void
dt_cg_logical_xor(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	uint_t lbl_next = dt_irlist_label(dlp);
	uint_t lbl_tail = dt_irlist_label(dlp);

	dt_cg_node(dnp->dn_left, dlp, drp);
	emit(dlp,  BPF_BRANCH_IMM(BPF_JEQ, dnp->dn_left->dn_reg, 0, lbl_next));
	dt_cg_setx(dlp, dnp->dn_left->dn_reg, 1);

	emitl(dlp, lbl_next,
		   BPF_NOP());

	dt_cg_node(dnp->dn_right, dlp, drp);
	emit(dlp,  BPF_BRANCH_IMM(BPF_JEQ, dnp->dn_right->dn_reg, 0, lbl_tail));
	dt_cg_setx(dlp, dnp->dn_right->dn_reg, 1);

	emitl(dlp, lbl_tail,
		   BPF_ALU64_REG(BPF_XOR, dnp->dn_left->dn_reg, dnp->dn_right->dn_reg));

	dt_regset_free(drp, dnp->dn_right->dn_reg);
	dnp->dn_reg = dnp->dn_left->dn_reg;
}

static void
dt_cg_logical_or(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	uint_t lbl_true = dt_irlist_label(dlp);
	uint_t lbl_false = dt_irlist_label(dlp);
	uint_t lbl_post = dt_irlist_label(dlp);

	dt_cg_node(dnp->dn_left, dlp, drp);
	emit(dlp,  BPF_BRANCH_IMM(BPF_JNE, dnp->dn_left->dn_reg, 0, lbl_true));
	dt_regset_free(drp, dnp->dn_left->dn_reg);

	dt_cg_node(dnp->dn_right, dlp, drp);
	emit(dlp,  BPF_BRANCH_IMM(BPF_JEQ, dnp->dn_right->dn_reg, 0, lbl_false));
	dnp->dn_reg = dnp->dn_right->dn_reg;

	emitl(dlp, lbl_true,
		   BPF_MOV_IMM(dnp->dn_reg, 1));
	emit(dlp,  BPF_JUMP(lbl_post));

	emitl(dlp, lbl_false,
		   BPF_MOV_IMM(dnp->dn_reg, 0));

	emitl(dlp, lbl_post,
		   BPF_NOP());
}

static void
dt_cg_logical_neg(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	uint_t lbl_zero = dt_irlist_label(dlp);
	uint_t lbl_post = dt_irlist_label(dlp);

	dt_cg_node(dnp->dn_child, dlp, drp);
	dnp->dn_reg = dnp->dn_child->dn_reg;

	emit(dlp,  BPF_BRANCH_IMM(BPF_JEQ, dnp->dn_reg, 0, lbl_zero));
	emit(dlp,  BPF_MOV_IMM(dnp->dn_reg, 0));
	emit(dlp,  BPF_JUMP(lbl_post));

	emitl(dlp, lbl_zero,
		   BPF_MOV_IMM(dnp->dn_reg, 1));

	emitl(dlp, lbl_post,
		   BPF_NOP());
}

static void
dt_cg_asgn_op(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_ident_t *idp;

	/*
	 * If we are performing a structure assignment of a translated type,
	 * we must instantiate all members and create a snapshot of the object
	 * in scratch space.  We allocs a chunk of memory, generate code for
	 * each member, and then set dnp->dn_reg to the scratch object address.
	 */
	if ((idp = dt_node_resolve(dnp->dn_right, DT_IDENT_XLSOU)) != NULL) {
#ifdef FIXME
		ctf_membinfo_t ctm;
		dt_xlator_t *dxp = idp->di_data;
		dt_node_t *mnp, dn, mn;
		int r1, r2;

		/*
		 * Create two fake dt_node_t's representing operator "." and a
		 * right-hand identifier child node.  These will be repeatedly
		 * modified according to each instantiated member so that we
		 * can pass them to dt_cg_store() and effect a member store.
		 */
		memset(&dn, 0, sizeof(dt_node_t));
		dn.dn_kind = DT_NODE_OP2;
		dn.dn_op = DT_TOK_DOT;
		dn.dn_left = dnp;
		dn.dn_right = &mn;

		memset(&mn, 0, sizeof(dt_node_t));
		mn.dn_kind = DT_NODE_IDENT;
		mn.dn_op = DT_TOK_IDENT;

		/*
		 * Allocate a register for our scratch data pointer.  First we
		 * set it to the size of our data structure, and then replace
		 * it with the result of an allocs of the specified size.
		 */
		if ((r1 = dt_regset_alloc(drp)) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

		dt_cg_setx(dlp, r1,
		    ctf_type_size(dxp->dx_dst_ctfp, dxp->dx_dst_base));

		instr = DIF_INSTR_ALLOCS(r1, r1);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

		/*
		 * When dt_cg_asgn_op() is called, we have already generated
		 * code for dnp->dn_right, which is the translator input.  We
		 * now associate this register with the translator's input
		 * identifier so it can be referenced during our member loop.
		 */
		dxp->dx_ident->di_flags |= DT_IDFLG_CGREG;
		dxp->dx_ident->di_id = dnp->dn_right->dn_reg;

		for (mnp = dxp->dx_members; mnp != NULL; mnp = mnp->dn_list) {
			/*
			 * Generate code for the translator member expression,
			 * and then cast the result to the member type.
			 */
			dt_cg_node(mnp->dn_membexpr, dlp, drp);
			mnp->dn_reg = mnp->dn_membexpr->dn_reg;
			dt_cg_typecast(mnp->dn_membexpr, mnp, dlp, drp);

			/*
			 * Ask CTF for the offset of the member so we can store
			 * to the appropriate offset.  This call has already
			 * been done once by the parser, so it should succeed.
			 */
			if (ctf_member_info(dxp->dx_dst_ctfp, dxp->dx_dst_base,
			    mnp->dn_membname, &ctm) == CTF_ERR) {
				yypcb->pcb_hdl->dt_ctferr =
				    ctf_errno(dxp->dx_dst_ctfp);
				longjmp(yypcb->pcb_jmpbuf, EDT_CTF);
			}

			/*
			 * If the destination member is at offset 0, store the
			 * result directly to r1 (the scratch buffer address).
			 * Otherwise allocate another temporary for the offset
			 * and add r1 to it before storing the result.
			 */
			if (ctm.ctm_offset != 0) {
				if ((r2 = dt_regset_alloc(drp)) == -1)
					longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

				/*
				 * Add the member offset rounded down to the
				 * nearest byte.  If the offset was not aligned
				 * on a byte boundary, this member is a bit-
				 * field and dt_cg_store() will handle masking.
				 */
				dt_cg_setx(dlp, r2, ctm.ctm_offset / NBBY);
				emit(dlp, BPF_ALU64_REG(BPF_ADD, r2, r1));

				dt_node_type_propagate(mnp, &dn);
				dn.dn_right->dn_string = mnp->dn_membname;
				dn.dn_reg = r2;

				dt_cg_store(mnp, dlp, drp, &dn);
				dt_regset_free(drp, r2);

			} else {
				dt_node_type_propagate(mnp, &dn);
				dn.dn_right->dn_string = mnp->dn_membname;
				dn.dn_reg = r1;

				dt_cg_store(mnp, dlp, drp, &dn);
			}

			dt_regset_free(drp, mnp->dn_reg);
		}

		dxp->dx_ident->di_flags &= ~DT_IDFLG_CGREG;
		dxp->dx_ident->di_id = 0;

		if (dnp->dn_right->dn_reg != -1)
			dt_regset_free(drp, dnp->dn_right->dn_reg);

		assert(dnp->dn_reg == dnp->dn_right->dn_reg);
		dnp->dn_reg = r1;
#else
		xyerror(D_UNKNOWN, "internal error -- no support for "
			"translated types yet\n");
#endif
	}

	/*
	 * If we are storing to a variable, generate an stv instruction from
	 * the variable specified by the identifier.  If we are storing to a
	 * memory address, generate code again for the left-hand side using
	 * DT_NF_REF to get the address, and then generate a store to it.
	 * In both paths, we assume dnp->dn_reg already has the new value.
	 */
	if (dnp->dn_left->dn_kind == DT_NODE_VAR) {
		idp = dt_ident_resolve(dnp->dn_left->dn_ident);

		if (idp->di_kind == DT_IDENT_ARRAY)
			dt_cg_arglist(idp, dnp->dn_left->dn_args, dlp, drp);

		dt_cg_store_var(dnp, dlp, drp, idp);
	} else {
		uint_t rbit = dnp->dn_left->dn_flags & DT_NF_REF;

		assert(dnp->dn_left->dn_flags & DT_NF_WRITABLE);
		assert(dnp->dn_left->dn_flags & DT_NF_LVALUE);

		dnp->dn_left->dn_flags |= DT_NF_REF; /* force pass-by-ref */

		dt_cg_node(dnp->dn_left, dlp, drp);
		dt_cg_store(dnp, dlp, drp, dnp->dn_left);
		dt_regset_free(drp, dnp->dn_left->dn_reg);

		dnp->dn_left->dn_flags &= ~DT_NF_REF;
		dnp->dn_left->dn_flags |= rbit;
	}
}

static void
dt_cg_assoc_op(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	ssize_t base;

	assert(dnp->dn_kind == DT_NODE_VAR);
	assert(!(dnp->dn_ident->di_flags & DT_IDFLG_LOCAL));
	assert(dnp->dn_args != NULL);

	dt_cg_arglist(dnp->dn_ident, dnp->dn_args, dlp, drp);

	if ((dnp->dn_reg = dt_regset_alloc(drp)) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	if (dnp->dn_ident->di_flags & DT_IDFLG_TLS)
		base = 0x2000;
	else
		base = 0x3000;

	dnp->dn_ident->di_flags |= DT_IDFLG_DIFR;
	emit(dlp, BPF_LOAD(BPF_DW, dnp->dn_reg, BPF_REG_FP, base + dnp->dn_ident->di_id));

	/*
	 * If the associative array is a pass-by-reference type, then we are
	 * loading its value as a pointer to either load or store through it.
	 * The array element in question may not have been faulted in yet, in
	 * which case DIF_OP_LD*AA will return zero.  We append an epilogue
	 * of instructions similar to the following:
	 *
	 *	  ld?aa	 id, %r1	! base ld?aa instruction above
	 *	  tst	 %r1		! start of epilogue
	 *   +--- bne	 label
	 *   |    setx	 size, %r1
	 *   |    allocs %r1, %r1
	 *   |    st?aa	 id, %r1
	 *   |    ld?aa	 id, %r1
	 *   v
	 * label: < rest of code >
	 *
	 * The idea is that we allocs a zero-filled chunk of scratch space and
	 * do a DIF_OP_ST*AA to fault in and initialize the array element, and
	 * then reload it to get the faulted-in address of the new variable
	 * storage.  This isn't cheap, but pass-by-ref associative array values
	 * are (thus far) uncommon and the allocs cost only occurs once.  If
	 * this path becomes important to DTrace users, we can improve things
	 * by adding a new DIF opcode to fault in associative array elements.
	 */
	if (dnp->dn_flags & DT_NF_REF) {
#ifdef FIXME
		uint_t stvop = op == DIF_OP_LDTAA ? DIF_OP_STTAA : DIF_OP_STGAA;
		uint_t label = dt_irlist_label(dlp);

		emit(dlp, BPF_BRANCH_IMM(BPF_JNE, dnp->dn_reg, 0, label));

		dt_cg_setx(dlp, dnp->dn_reg, dt_node_type_size(dnp));
		instr = DIF_INSTR_ALLOCS(dnp->dn_reg, dnp->dn_reg);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

		dnp->dn_ident->di_flags |= DT_IDFLG_DIFW;
		instr = DIF_INSTR_STV(stvop, dnp->dn_ident->di_id, dnp->dn_reg);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

		instr = DIF_INSTR_LDV(op, dnp->dn_ident->di_id, dnp->dn_reg);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

		emitl(dlp, label,
			   BPF_NOP());
#else
		xyerror(D_UNKNOWN, "internal error -- no support for "
			"translated types yet\n");
#endif
	}
}

static void
dt_cg_array_op(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_probe_t *prp = yypcb->pcb_probe;
	uintmax_t saved = dnp->dn_args->dn_value;
	dt_ident_t *idp = dnp->dn_ident;

	ssize_t base;
	size_t size;
	int n;

	assert(dnp->dn_kind == DT_NODE_VAR);
	assert(!(idp->di_flags & DT_IDFLG_LOCAL));

	assert(dnp->dn_args->dn_kind == DT_NODE_INT);
	assert(dnp->dn_args->dn_list == NULL);

	/*
	 * If this is a reference in the args[] array, temporarily modify the
	 * array index according to the static argument mapping (if any),
	 * unless the argument reference is provided by a dynamic translator.
	 * If we're using a dynamic translator for args[], then just set dn_reg
	 * to an invalid reg and return: DIF_OP_XLARG will fetch the arg later.
	 */
	if (idp->di_id == DIF_VAR_ARGS) {
		if ((idp->di_kind == DT_IDENT_XLPTR ||
		    idp->di_kind == DT_IDENT_XLSOU) &&
		    dt_xlator_dynamic(idp->di_data)) {
			dnp->dn_reg = -1;
			return;
		}
		dnp->dn_args->dn_value = prp->mapping[saved];
	}

	dt_cg_node(dnp->dn_args, dlp, drp);
	dnp->dn_args->dn_value = saved;

	dnp->dn_reg = dnp->dn_args->dn_reg;

	if (idp->di_flags & DT_IDFLG_TLS)
		base = 0x2000;
	else
		base = 0x3000;

	idp->di_flags |= DT_IDFLG_DIFR;
	emit(dlp, BPF_LOAD(BPF_DW, dnp->dn_reg, BPF_REG_FP, base + dnp->dn_ident->di_id));

	/*
	 * If this is a reference to the args[] array, we need to take the
	 * additional step of explicitly eliminating any bits larger than the
	 * type size: the DIF interpreter in the kernel will always give us
	 * the raw (64-bit) argument value, and any bits larger than the type
	 * size may be junk.  As a practical matter, this arises only on 64-bit
	 * architectures and only when the argument index is larger than the
	 * number of arguments passed directly to DTrace: if a 8-, 16- or
	 * 32-bit argument must be retrieved from the stack, it is possible
	 * (and it some cases, likely) that the upper bits will be garbage.
	 */
	if (idp->di_id != DIF_VAR_ARGS || !dt_node_is_scalar(dnp))
		return;

	if ((size = dt_node_type_size(dnp)) == sizeof(uint64_t))
		return;

	assert(size < sizeof(uint64_t));
	n = sizeof(uint64_t) * NBBY - size * NBBY;

	emit(dlp, BPF_ALU64_IMM(BPF_LSH, dnp->dn_reg, n));
	emit(dlp, BPF_ALU64_REG((dnp->dn_flags & DT_NF_SIGNED) ? BPF_ARSH : BPF_RSH, dnp->dn_reg, n));
}

static void
dt_cg_subr_speculation(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	TRACE_REGSET("    subr-speculation:Begin");
	dnp->dn_reg = dt_regset_alloc(drp);
	emit(dlp, BPF_MOV_IMM(dnp->dn_reg, 0));
	TRACE_REGSET("    subr-speculation:End  ");
}

static void
dt_cg_subr_strlen(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	TRACE_REGSET("    subr-strlen:Begin");
	dt_cg_node(dnp->dn_args, dlp, drp);
	dnp->dn_reg = dnp->dn_args->dn_reg;
	dt_cg_strlen(dlp, drp, dnp->dn_reg, dnp->dn_args->dn_reg);
	TRACE_REGSET("    subr-strlen:End  ");
}

typedef void dt_cg_subr_f(dt_node_t *, dt_irlist_t *, dt_regset_t *);

static dt_cg_subr_f *_dt_cg_subr[DIF_SUBR_MAX + 1] = {
	[DIF_SUBR_RAND]			= NULL,
	[DIF_SUBR_MUTEX_OWNED]		= NULL,
	[DIF_SUBR_MUTEX_OWNER]		= NULL,
	[DIF_SUBR_MUTEX_TYPE_ADAPTIVE]	= NULL,
	[DIF_SUBR_MUTEX_TYPE_SPIN]	= NULL,
	[DIF_SUBR_RW_READ_HELD]		= NULL,
	[DIF_SUBR_RW_WRITE_HELD]	= NULL,
	[DIF_SUBR_RW_ISWRITER]		= NULL,
	[DIF_SUBR_COPYIN]		= NULL,
	[DIF_SUBR_COPYINSTR]		= NULL,
	[DIF_SUBR_SPECULATION]		= &dt_cg_subr_speculation,
	[DIF_SUBR_PROGENYOF]		= NULL,
	[DIF_SUBR_STRLEN]		= &dt_cg_subr_strlen,
	[DIF_SUBR_COPYOUT]		= NULL,
	[DIF_SUBR_COPYOUTSTR]		= NULL,
	[DIF_SUBR_ALLOCA]		= NULL,
	[DIF_SUBR_BCOPY]		= NULL,
	[DIF_SUBR_COPYINTO]		= NULL,
	[DIF_SUBR_MSGDSIZE]		= NULL,
	[DIF_SUBR_MSGSIZE]		= NULL,
	[DIF_SUBR_GETMAJOR]		= NULL,
	[DIF_SUBR_GETMINOR]		= NULL,
	[DIF_SUBR_DDI_PATHNAME]		= NULL,
	[DIF_SUBR_STRJOIN]		= NULL,
	[DIF_SUBR_LLTOSTR]		= NULL,
	[DIF_SUBR_BASENAME]		= NULL,
	[DIF_SUBR_DIRNAME]		= NULL,
	[DIF_SUBR_CLEANPATH]		= NULL,
	[DIF_SUBR_STRCHR]		= NULL,
	[DIF_SUBR_STRRCHR]		= NULL,
	[DIF_SUBR_STRSTR]		= NULL,
	[DIF_SUBR_STRTOK]		= NULL,
	[DIF_SUBR_SUBSTR]		= NULL,
	[DIF_SUBR_INDEX]		= NULL,
	[DIF_SUBR_RINDEX]		= NULL,
	[DIF_SUBR_HTONS]		= NULL,
	[DIF_SUBR_HTONL]		= NULL,
	[DIF_SUBR_HTONLL]		= NULL,
	[DIF_SUBR_NTOHS]		= NULL,
	[DIF_SUBR_NTOHL]		= NULL,
	[DIF_SUBR_NTOHLL]		= NULL,
	[DIF_SUBR_INET_NTOP]		= NULL,
	[DIF_SUBR_INET_NTOA]		= NULL,
	[DIF_SUBR_INET_NTOA6]		= NULL,
	[DIF_SUBR_D_PATH]		= NULL,
	[DIF_SUBR_LINK_NTOP]		= NULL,
};

static void
dt_cg_call_subr(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_ident_t	*idp = dnp->dn_ident;
	dt_cg_subr_f	*fun;

	if (idp->di_kind != DT_IDENT_FUNC)
		dnerror(dnp, D_CG_EXPR, "%s %s( ) may not be called from a D "
					"expression (D program context "
					"required)\n",
			dt_idkind_name(idp->di_kind), idp->di_name);

	assert(idp->di_id > 0 && idp->di_id <= DIF_SUBR_MAX);

	fun = _dt_cg_subr[idp->di_id];
	if (fun == NULL)
		dnerror(dnp, D_FUNC_UNDEF, "unimplemented subroutine: %s\n",
			idp->di_name);
	fun(dnp, dlp, drp);
}

/*
 * Generate code for an inlined variable reference.  Inlines can be used to
 * define either scalar or associative array substitutions.  For scalars, we
 * simply generate code for the parse tree saved in the identifier's din_root,
 * and then cast the resulting expression to the inline's declaration type.
 * For arrays, we take the input parameter subtrees from dnp->dn_args and
 * temporarily store them in the din_root of each din_argv[i] identifier,
 * which are themselves inlines and were set up for us by the parser.  The
 * result is that any reference to the inlined parameter inside the top-level
 * din_root will turn into a recursive call to dt_cg_inline() for a scalar
 * inline whose din_root will refer to the subtree pointed to by the argument.
 */
static void
dt_cg_inline(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_ident_t *idp = dnp->dn_ident;
	dt_idnode_t *inp = idp->di_iarg;

	dt_idnode_t *pinp;
	dt_node_t *pnp;
	int i;

	assert(idp->di_flags & DT_IDFLG_INLINE);
	assert(idp->di_ops == &dt_idops_inline);

	if (idp->di_kind == DT_IDENT_ARRAY) {
		for (i = 0, pnp = dnp->dn_args;
		    pnp != NULL; pnp = pnp->dn_list, i++) {
			if (inp->din_argv[i] != NULL) {
				pinp = inp->din_argv[i]->di_iarg;
				pinp->din_root = pnp;
			}
		}
	}

	dt_cg_node(inp->din_root, dlp, drp);
	dnp->dn_reg = inp->din_root->dn_reg;
	dt_cg_typecast(inp->din_root, dnp, dlp, drp);

	if (idp->di_kind == DT_IDENT_ARRAY) {
		for (i = 0; i < inp->din_argc; i++) {
			pinp = inp->din_argv[i]->di_iarg;
			pinp->din_root = NULL;
		}
	}
}

static void
dt_cg_node(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	ctf_file_t *ctfp = dnp->dn_ctfp;
	ctf_file_t *octfp;
	ctf_membinfo_t m;
	ctf_id_t type;
	dt_ident_t *idp;
	ssize_t stroff;

	switch (dnp->dn_op) {
	case DT_TOK_COMMA:
		dt_cg_node(dnp->dn_left, dlp, drp);
		dt_regset_free(drp, dnp->dn_left->dn_reg);
		dt_cg_node(dnp->dn_right, dlp, drp);
		dnp->dn_reg = dnp->dn_right->dn_reg;
		break;

	case DT_TOK_ASGN:
		dt_cg_node(dnp->dn_right, dlp, drp);
		dnp->dn_reg = dnp->dn_right->dn_reg;
		dt_cg_asgn_op(dnp, dlp, drp);
		break;

	case DT_TOK_ADD_EQ:
		dt_cg_arithmetic_op(dnp, dlp, drp, BPF_ADD);
		dt_cg_asgn_op(dnp, dlp, drp);
		break;

	case DT_TOK_SUB_EQ:
		dt_cg_arithmetic_op(dnp, dlp, drp, BPF_SUB);
		dt_cg_asgn_op(dnp, dlp, drp);
		break;

	case DT_TOK_MUL_EQ:
		dt_cg_arithmetic_op(dnp, dlp, drp, BPF_MUL);
		dt_cg_asgn_op(dnp, dlp, drp);
		break;

	case DT_TOK_DIV_EQ:
		dt_cg_arithmetic_op(dnp, dlp, drp, BPF_DIV);
		dt_cg_asgn_op(dnp, dlp, drp);
		break;

	case DT_TOK_MOD_EQ:
		dt_cg_arithmetic_op(dnp, dlp, drp, BPF_MOD);
		dt_cg_asgn_op(dnp, dlp, drp);
		break;

	case DT_TOK_AND_EQ:
		dt_cg_arithmetic_op(dnp, dlp, drp, BPF_AND);
		dt_cg_asgn_op(dnp, dlp, drp);
		break;

	case DT_TOK_XOR_EQ:
		dt_cg_arithmetic_op(dnp, dlp, drp, BPF_XOR);
		dt_cg_asgn_op(dnp, dlp, drp);
		break;

	case DT_TOK_OR_EQ:
		dt_cg_arithmetic_op(dnp, dlp, drp, BPF_OR);
		dt_cg_asgn_op(dnp, dlp, drp);
		break;

	case DT_TOK_LSH_EQ:
		dt_cg_arithmetic_op(dnp, dlp, drp, BPF_LSH);
		dt_cg_asgn_op(dnp, dlp, drp);
		break;

	case DT_TOK_RSH_EQ:
		dt_cg_arithmetic_op(dnp, dlp, drp,
		    (dnp->dn_flags & DT_NF_SIGNED) ? BPF_ARSH : BPF_RSH);
		dt_cg_asgn_op(dnp, dlp, drp);
		break;

	case DT_TOK_QUESTION:
		dt_cg_ternary_op(dnp, dlp, drp);
		break;

	case DT_TOK_LOR:
		dt_cg_logical_or(dnp, dlp, drp);
		break;

	case DT_TOK_LXOR:
		dt_cg_logical_xor(dnp, dlp, drp);
		break;

	case DT_TOK_LAND:
		dt_cg_logical_and(dnp, dlp, drp);
		break;

	case DT_TOK_BOR:
		dt_cg_arithmetic_op(dnp, dlp, drp, BPF_OR);
		break;

	case DT_TOK_XOR:
		dt_cg_arithmetic_op(dnp, dlp, drp, BPF_XOR);
		break;

	case DT_TOK_BAND:
		dt_cg_arithmetic_op(dnp, dlp, drp, BPF_AND);
		break;

	case DT_TOK_EQU:
		dt_cg_compare_op(dnp, dlp, drp, BPF_JEQ);
		break;

	case DT_TOK_NEQ:
		dt_cg_compare_op(dnp, dlp, drp, BPF_JNE);
		break;

	case DT_TOK_LT:
		dt_cg_compare_op(dnp, dlp, drp,
		    dt_cg_compare_signed(dnp) ? BPF_JSLT : BPF_JLT);
		break;

	case DT_TOK_LE:
		dt_cg_compare_op(dnp, dlp, drp,
		    dt_cg_compare_signed(dnp) ? BPF_JSLE : BPF_JLE);
		break;

	case DT_TOK_GT:
		dt_cg_compare_op(dnp, dlp, drp,
		    dt_cg_compare_signed(dnp) ? BPF_JSGT : BPF_JGT);
		break;

	case DT_TOK_GE:
		dt_cg_compare_op(dnp, dlp, drp,
		    dt_cg_compare_signed(dnp) ? BPF_JSGE : BPF_JGE);
		break;

	case DT_TOK_LSH:
		dt_cg_arithmetic_op(dnp, dlp, drp, BPF_LSH);
		break;

	case DT_TOK_RSH:
		dt_cg_arithmetic_op(dnp, dlp, drp,
		    (dnp->dn_flags & DT_NF_SIGNED) ? BPF_ARSH : BPF_RSH);
		break;

	case DT_TOK_ADD:
		dt_cg_arithmetic_op(dnp, dlp, drp, BPF_ADD);
		break;

	case DT_TOK_SUB:
		dt_cg_arithmetic_op(dnp, dlp, drp, BPF_SUB);
		break;

	case DT_TOK_MUL:
		dt_cg_arithmetic_op(dnp, dlp, drp, BPF_MUL);
		break;

	case DT_TOK_DIV:
		dt_cg_arithmetic_op(dnp, dlp, drp, BPF_DIV);
		break;

	case DT_TOK_MOD:
		dt_cg_arithmetic_op(dnp, dlp, drp, BPF_MOD);
		break;

	case DT_TOK_LNEG:
		dt_cg_logical_neg(dnp, dlp, drp);
		break;

	case DT_TOK_BNEG:
		dt_cg_node(dnp->dn_child, dlp, drp);
		dnp->dn_reg = dnp->dn_child->dn_reg;
		emit(dlp, BPF_ALU64_IMM(BPF_XOR, dnp->dn_reg, -1));
		break;

	case DT_TOK_PREINC:
		dt_cg_prearith_op(dnp, dlp, drp, BPF_ADD);
		break;

	case DT_TOK_POSTINC:
		dt_cg_postarith_op(dnp, dlp, drp, BPF_ADD);
		break;

	case DT_TOK_PREDEC:
		dt_cg_prearith_op(dnp, dlp, drp, BPF_SUB);
		break;

	case DT_TOK_POSTDEC:
		dt_cg_postarith_op(dnp, dlp, drp, BPF_SUB);
		break;

	case DT_TOK_IPOS:
		dt_cg_node(dnp->dn_child, dlp, drp);
		dnp->dn_reg = dnp->dn_child->dn_reg;
		break;

	case DT_TOK_INEG:
		dt_cg_node(dnp->dn_child, dlp, drp);
		dnp->dn_reg = dnp->dn_child->dn_reg;

		emit(dlp, BPF_NEG_REG(dnp->dn_reg));
		break;

	case DT_TOK_DEREF:
		dt_cg_node(dnp->dn_child, dlp, drp);
		dnp->dn_reg = dnp->dn_child->dn_reg;

		if (!(dnp->dn_flags & DT_NF_REF)) {
			uint_t	ubit;

			/*
			 * Save and restore DT_NF_USERLAND across dt_cg_load():
			 * we need the sign bit from dnp and the user bit from
			 * dnp->dn_child in order to get the proper opcode.
			 */
			ubit = dnp->dn_flags & DT_NF_USERLAND;
			dnp->dn_flags |=
			    (dnp->dn_child->dn_flags & DT_NF_USERLAND);

			dt_cg_check_notnull(dlp, drp, dnp->dn_reg);

			/* FIXME: Does not handled signed or userland */
			emit(dlp, BPF_LOAD(dt_cg_load(dnp, ctfp, dnp->dn_type), dnp->dn_reg, dnp->dn_reg, 0));

			dnp->dn_flags &= ~DT_NF_USERLAND;
			dnp->dn_flags |= ubit;
		}
		break;

	case DT_TOK_ADDROF: {
		uint_t rbit = dnp->dn_child->dn_flags & DT_NF_REF;

		dnp->dn_child->dn_flags |= DT_NF_REF; /* force pass-by-ref */
		dt_cg_node(dnp->dn_child, dlp, drp);
		dnp->dn_reg = dnp->dn_child->dn_reg;

		dnp->dn_child->dn_flags &= ~DT_NF_REF;
		dnp->dn_child->dn_flags |= rbit;
		break;
	}

	case DT_TOK_SIZEOF: {
		size_t size = dt_node_sizeof(dnp->dn_child);

		if ((dnp->dn_reg = dt_regset_alloc(drp)) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

		assert(size != 0);
		dt_cg_setx(dlp, dnp->dn_reg, size);
		break;
	}

	case DT_TOK_STRINGOF:
		dt_cg_node(dnp->dn_child, dlp, drp);
		dnp->dn_reg = dnp->dn_child->dn_reg;
		break;

	case DT_TOK_XLATE:
		/*
		 * An xlate operator appears in either an XLATOR, indicating a
		 * reference to a dynamic translator, or an OP2, indicating
		 * use of the xlate operator in the user's program.  For the
		 * dynamic case, generate an xlate opcode with a reference to
		 * the corresponding member, pre-computed for us in dn_members.
		 */
		if (dnp->dn_kind == DT_NODE_XLATOR) {
#ifdef FIXME
			dt_xlator_t *dxp = dnp->dn_xlator;

			assert(dxp->dx_ident->di_flags & DT_IDFLG_CGREG);
			assert(dxp->dx_ident->di_id != 0);

			if ((dnp->dn_reg = dt_regset_alloc(drp)) == -1)
				longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

			if (dxp->dx_arg == -1) {
				emit(dlp, BPF_MOV_REG(dnp->dn_reg, dxp->dx_ident->di_id));
				op = DIF_OP_XLATE;
			} else
				op = DIF_OP_XLARG;

			instr = DIF_INSTR_XLATE(op, 0, dnp->dn_reg);
			dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

			dlp->dl_last->di_extern = dnp->dn_xmember;
#else
			xyerror(D_UNKNOWN, "internal error -- cg does not "
				"support xlate yet\n");
#endif
			break;
		}

		assert(dnp->dn_kind == DT_NODE_OP2);
		dt_cg_node(dnp->dn_right, dlp, drp);
		dnp->dn_reg = dnp->dn_right->dn_reg;
		break;

	case DT_TOK_LPAR:
		dt_cg_node(dnp->dn_right, dlp, drp);
		dnp->dn_reg = dnp->dn_right->dn_reg;
		dt_cg_typecast(dnp->dn_right, dnp, dlp, drp);
		break;

	case DT_TOK_PTR:
	case DT_TOK_DOT: {

		assert(dnp->dn_right->dn_kind == DT_NODE_IDENT);
		dt_cg_node(dnp->dn_left, dlp, drp);
		dt_cg_check_notnull(dlp, drp, dnp->dn_left->dn_reg);

		/*
		 * If the left-hand side of PTR or DOT is a dynamic variable,
		 * we expect it to be the output of a D translator.   In this
		 * case, we look up the parse tree corresponding to the member
		 * that is being accessed and run the code generator over it.
		 * We then cast the result as if by the assignment operator.
		 */
		if ((idp = dt_node_resolve(
		    dnp->dn_left, DT_IDENT_XLSOU)) != NULL ||
		    (idp = dt_node_resolve(
		    dnp->dn_left, DT_IDENT_XLPTR)) != NULL) {
			dt_xlator_t *dxp;
			dt_node_t *mnp;

			dxp = idp->di_data;
			mnp = dt_xlator_member(dxp, dnp->dn_right->dn_string);
			assert(mnp != NULL);

			dxp->dx_ident->di_flags |= DT_IDFLG_CGREG;
			dxp->dx_ident->di_id = dnp->dn_left->dn_reg;

			dt_cg_node(mnp->dn_membexpr, dlp, drp);
			dnp->dn_reg = mnp->dn_membexpr->dn_reg;
			dt_cg_typecast(mnp->dn_membexpr, dnp, dlp, drp);

			dxp->dx_ident->di_flags &= ~DT_IDFLG_CGREG;
			dxp->dx_ident->di_id = 0;

			if (dnp->dn_left->dn_reg != -1)
				dt_regset_free(drp, dnp->dn_left->dn_reg);
			break;
		}

		ctfp = dnp->dn_left->dn_ctfp;
		type = ctf_type_resolve(ctfp, dnp->dn_left->dn_type);

		if (dnp->dn_op == DT_TOK_PTR) {
			type = ctf_type_reference(ctfp, type);
			type = ctf_type_resolve(ctfp, type);
		}

		if ((ctfp = dt_cg_membinfo(octfp = ctfp, type,
		    dnp->dn_right->dn_string, &m)) == NULL) {
			yypcb->pcb_hdl->dt_ctferr = ctf_errno(octfp);
			longjmp(yypcb->pcb_jmpbuf, EDT_CTF);
		}

		if (m.ctm_offset != 0) {
			/*
			 * If the offset is not aligned on a byte boundary, it
			 * is a bit-field member and we will extract the value
			 * bits below after we generate the appropriate load.
			 */
			emit(dlp, BPF_ALU64_IMM(BPF_ADD, dnp->dn_left->dn_reg, m.ctm_offset / NBBY));
		}

		if (!(dnp->dn_flags & DT_NF_REF)) {
			uint_t ubit;

			/*
			 * Save and restore DT_NF_USERLAND across dt_cg_load():
			 * we need the sign bit from dnp and the user bit from
			 * dnp->dn_left in order to get the proper opcode.
			 */
			ubit = dnp->dn_flags & DT_NF_USERLAND;
			dnp->dn_flags |=
			    (dnp->dn_left->dn_flags & DT_NF_USERLAND);

			/* FIXME: Does not handle signed and userland */
			emit(dlp, BPF_LOAD(dt_cg_load(dnp, ctfp, m.ctm_type), dnp->dn_left->dn_reg, dnp->dn_left->dn_reg, 0));

			dnp->dn_flags &= ~DT_NF_USERLAND;
			dnp->dn_flags |= ubit;

			if (dnp->dn_flags & DT_NF_BITFIELD)
				dt_cg_field_get(dnp, dlp, drp, ctfp, &m);
		}

		dnp->dn_reg = dnp->dn_left->dn_reg;
		break;
	}

	case DT_TOK_STRING:
		if ((dnp->dn_reg = dt_regset_alloc(drp)) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

		assert(dnp->dn_kind == DT_NODE_STRING);
		stroff = dt_strtab_insert(yypcb->pcb_hdl->dt_ccstab,
					  dnp->dn_string);

		if (stroff == -1L)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOMEM);
		if (stroff > DIF_STROFF_MAX)
			longjmp(yypcb->pcb_jmpbuf, EDT_STR2BIG);

		/*
		 * Calculate the address of the string data in the 'strtab' BPF
		 * map.
		 */
		emit(dlp, BPF_LOAD(BPF_DW, dnp->dn_reg, BPF_REG_FP, DT_STK_DCTX));
		emit(dlp, BPF_LOAD(BPF_DW, dnp->dn_reg, dnp->dn_reg, DCTX_STRTAB));
		emit(dlp, BPF_ALU64_IMM(BPF_ADD, dnp->dn_reg, stroff));
		break;

	case DT_TOK_IDENT:
		/*
		 * If the specified identifier is a variable on which we have
		 * set the code generator register flag, then this variable
		 * has already had code generated for it and saved in di_id.
		 * Allocate a new register and copy the existing value to it.
		 */
		if (dnp->dn_kind == DT_NODE_VAR &&
		    (dnp->dn_ident->di_flags & DT_IDFLG_CGREG)) {
			if ((dnp->dn_reg = dt_regset_alloc(drp)) == -1)
				longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
			emit(dlp, BPF_MOV_REG(dnp->dn_reg, dnp->dn_ident->di_id));
			break;
		}

		/*
		 * Identifiers can represent function calls, variable refs, or
		 * symbols.  First we check for inlined variables, and handle
		 * them by generating code for the inline parse tree.
		 */
		if (dnp->dn_kind == DT_NODE_VAR &&
		    (dnp->dn_ident->di_flags & DT_IDFLG_INLINE)) {
			dt_cg_inline(dnp, dlp, drp);
			break;
		}

		switch (dnp->dn_kind) {
		case DT_NODE_FUNC:
			dt_cg_call_subr(dnp, dlp, drp);
			break;

		case DT_NODE_VAR: {
			if (dnp->dn_ident->di_kind == DT_IDENT_XLSOU ||
			    dnp->dn_ident->di_kind == DT_IDENT_XLPTR) {
				/*
				 * This can only happen if we have translated
				 * args[].  See dt_idcook_args() for details.
				 */
				assert(dnp->dn_ident->di_id == DIF_VAR_ARGS);
				dt_cg_array_op(dnp, dlp, drp);
				break;
			}

			if (dnp->dn_ident->di_kind == DT_IDENT_ARRAY) {
				if (dnp->dn_ident->di_id > DIF_VAR_ARRAY_MAX)
					dt_cg_assoc_op(dnp, dlp, drp);
				else
					dt_cg_array_op(dnp, dlp, drp);
				break;
			}

			dt_cg_load_var(dnp, dlp, drp);

			break;
		}

		case DT_NODE_SYM: {
			dtrace_hdl_t *dtp = yypcb->pcb_hdl;
			dtrace_syminfo_t *sip = dnp->dn_ident->di_data;
			GElf_Sym sym;

			if (dtrace_lookup_by_name(dtp, sip->object, sip->name,
						  &sym, NULL) == -1) {
				xyerror(D_UNKNOWN, "cg failed for symbol %s`%s:"
					" %s\n", sip->object, sip->name,
					dtrace_errmsg(dtp, dtrace_errno(dtp)));
			}

			if ((dnp->dn_reg = dt_regset_alloc(drp)) == -1)
				longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

			dt_cg_xsetx(dlp, dnp->dn_ident,
			    DT_LBL_NONE, dnp->dn_reg, sym.st_value);

			if (!(dnp->dn_flags & DT_NF_REF)) {
				/* FIXME: NO signed or userland yet */
				emit(dlp, BPF_LOAD(dt_cg_load(dnp, ctfp, dnp->dn_type), dnp->dn_reg, dnp->dn_reg, 0));
			}
			break;
		}

		default:
			xyerror(D_UNKNOWN, "internal error -- node type %u is "
			    "not valid for an identifier\n", dnp->dn_kind);
		}
		break;

	case DT_TOK_INT:
		if ((dnp->dn_reg = dt_regset_alloc(drp)) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

		if (dnp->dn_value > INT32_MAX)
			dt_cg_setx(dlp, dnp->dn_reg, dnp->dn_value);
		else
			emit(dlp, BPF_MOV_IMM(dnp->dn_reg, dnp->dn_value));
		break;

	default:
		xyerror(D_UNKNOWN, "internal error -- token type %u is not a "
		    "valid D compilation token\n", dnp->dn_op);
	}
}

/*
 * Macro to set the storage data (offset and size) for the aggregation
 * identifier (if not set yet).
 *
 * We make room for a latch sequence number of sizeof(uint64_t).
 *
 * If DT_AGG_NUM_COPIES==2, we consume twice the required data size for
 * a dual-copy mechanism to provide lockless, write-wait-free operation.
 */
#define DT_CG_AGG_SET_STORAGE(aid, sz) \
	do { \
		if ((aid)->di_offset == -1) \
			dt_ident_set_storage((aid), sizeof(uint64_t), \
					     sizeof(uint64_t) + \
					     DT_AGG_NUM_COPIES * (sz)); \
	} while (0)

#if DT_AGG_NUM_COPIES == 1
/*
 * Return a register that holds a pointer to the aggregation data to be
 * updated.
 *
 * We update the latch seqcount (first value in the aggregation) to
 * signal that the aggregation has data.  The location of data for the
 * given aggregation is stored in the register returned from this function.
 */
static int
dt_cg_agg_buf_prepare(dt_ident_t *aid, int size, dt_irlist_t *dlp,
		      dt_regset_t *drp)
{
	int		rptr;

	TRACE_REGSET("            Prep: Begin");

	dt_regset_xalloc(drp, BPF_REG_0);
	rptr = dt_regset_alloc(drp);
	assert(rptr != -1);

	/*
	 *	ptr = dctx->agg;	// lddw %rptr, [%fp + DT_STK_DCTX]
	 *				// lddw %rptr, [%rptr + DCTX_AGG]
	 */
	emit(dlp, BPF_LOAD(BPF_DW, rptr, BPF_REG_FP, DT_STK_DCTX));
	emit(dlp, BPF_LOAD(BPF_DW, rptr, rptr, DCTX_AGG));

	/*
	 *	*((uint64_t *)(ptr + aid->di_offset))++;
	 *				// mov %r0, 1
	 *				// xadd [%rptr + aid->di_offset], %r0
	 *      ptr += aid->di_offset + sizeof(uint64_t);
	 *				// add %rptr, aid->di_offset +
	 *				//	      sizeof(uint64_t)
	 */
	emit(dlp, BPF_MOV_IMM(BPF_REG_0, 1));
	emit(dlp, BPF_XADD_REG(BPF_DW, rptr, aid->di_offset, BPF_REG_0));
	emit(dlp, BPF_ALU64_IMM(BPF_ADD, rptr, aid->di_offset + sizeof(uint64_t)));

	dt_regset_free(drp, BPF_REG_0);

	TRACE_REGSET("            Prep: End  ");

	return rptr;
}
#else
/*
 * Prepare the aggregation buffer for updating for a specific aggregation, and
 * return a register that holds a pointer to the aggregation data to be
 * updated.
 *
 * We update the latch seqcount (first value in the aggregation) to
 * signal that reads should be directed to the alternate copy of the data.  We
 * then determine the location of data for the given aggregation that can be
 * updated.  This value is stored in the register returned from this function.
 */
static int
dt_cg_agg_buf_prepare(dt_ident_t *aid, int size, dt_irlist_t *dlp,
		      dt_regset_t *drp)
{
	int		ragd, roff;

	TRACE_REGSET("            Prep: Begin");

	dt_regset_xalloc(drp, BPF_REG_0);
	ragd = dt_regset_alloc(drp);
	roff = dt_regset_alloc(drp);
	assert(ragd != -1 && roff != -1);

	/*
	 *	agd = dctx->agg;	// lddw %r0, [%fp + DT_STK_DCTX]
	 *				// lddw %ragd, [%r0 + DCTX_AGG]
	 *	agd += aid->di_offset	// %ragd += aid->di_offset
	 */
	emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_FP, DT_STK_DCTX));
	emit(dlp, BPF_LOAD(BPF_DW, ragd, BPF_REG_0, DCTX_AGG));
	emit(dlp, BPF_ALU64_IMM(BPF_ADD, ragd, aid->di_offset));

	/*
	 *	off = (*agd & 1) * size	// lddw %roff, [%ragd + 0]
	 *	      + sizeof(uint64_t);	// and %roff, 1
	 *				// mul %roff, size
	 *				// add %roff, sizeof(uint64_t)
	 *	(*agd)++;		// mov %r0, 1
	 *				// xadd [%ragd + 0], %r0
	 *	agd += off;		// add %ragd, %roff
	 */
	emit(dlp, BPF_LOAD(BPF_DW, roff, ragd, 0));
	emit(dlp, BPF_ALU64_IMM(BPF_AND, roff, 1));
	emit(dlp, BPF_ALU64_IMM(BPF_MUL, roff, size));
	emit(dlp, BPF_ALU64_IMM(BPF_ADD, roff, sizeof(uint64_t)));
	emit(dlp, BPF_MOV_IMM(BPF_REG_0, 1));
	emit(dlp, BPF_XADD_REG(BPF_DW, ragd, 0, BPF_REG_0));
	emit(dlp, BPF_ALU64_REG(BPF_ADD, ragd, roff));

	dt_regset_free(drp, roff);
	dt_regset_free(drp, BPF_REG_0);

	TRACE_REGSET("            Prep: End  ");

	return ragd;
}
#endif

#define DT_CG_AGG_IMPL(aid, sz, dlp, drp, f, ...) \
	do {								\
		int	i, dreg;					\
									\
		TRACE_REGSET("        Upd: Begin ");			\
									\
		for (i = 0; i < DT_AGG_NUM_COPIES; i++) {		\
			if (i == 1)					\
				TRACE_REGSET("        Upd: Switch");	\
									\
			dreg = dt_cg_agg_buf_prepare((aid), (sz), (dlp), (drp));\
									\
			(f)((dlp), (drp), dreg, ## __VA_ARGS__);	\
			dt_regset_free((drp), dreg);			\
		}							\
									\
		TRACE_REGSET("        Upd: End   ");			\
	} while (0)

static dt_node_t *
dt_cg_agg_opt_incr(dt_node_t *dnp, dt_node_t *lastarg, const char *fn,
		   int argmax)
{
	dt_node_t	*incr;

	incr = lastarg != NULL ? lastarg->dn_list : NULL;
	if (incr == NULL)
		return NULL;

	if (!dt_node_is_scalar(incr)) {
		dnerror(dnp, D_PROTO_ARG, "%s( ) increment value (argument "
					  "#%d) must be of scalar type\n",
			fn, argmax);
	}

	if (incr->dn_list != NULL) {
		int	argc = argmax;

		for (dnp = incr->dn_list; dnp != NULL; dnp = dnp->dn_list)
			argc++;

		dnerror(incr, D_PROTO_LEN, "%s( ) prototype mismatch: %d args "
					   "passed, at most %d expected",
			fn, argc, argmax);
	}

	return incr;
}

static void
dt_cg_agg_avg_impl(dt_irlist_t *dlp, dt_regset_t *drp, int dreg, int vreg)
{
	TRACE_REGSET("            Impl: Begin");

	/*
	 * uint64_t dst[2], val;
	 * char *dreg = dst, *vreg = &val;
	 *				//     (%rd = dreg -- agg data ptr)
	 *				//     (%rv = vreg -- value)
	 *	uint64_t *dst = dreg;
	 *
	 *	dst[0]++;		// mov %r0, 1
	 *				// xadd [%rd + 0], %r0
	 *	dst[1] += vreg;		// xadd [%rd + 8], %rv
	 */

	/* dst[0]++ */
	dt_regset_xalloc(drp, BPF_REG_0);
	emit(dlp, BPF_MOV_IMM(BPF_REG_0, 1));
	emit(dlp, BPF_XADD_REG(BPF_DW, dreg, 0, BPF_REG_0));
	dt_regset_free(drp, BPF_REG_0);

	/* dst[1] += val */
	emit(dlp, BPF_XADD_REG(BPF_DW, dreg, 8, vreg));

	TRACE_REGSET("            Impl: End  ");
}

static void
dt_cg_agg_avg(dt_pcb_t *pcb, dt_ident_t *aid, dt_node_t *dnp,
	      dt_irlist_t *dlp, dt_regset_t *drp)
{
	int	sz = 2 * sizeof(uint64_t);

	DT_CG_AGG_SET_STORAGE(aid, sz);

	TRACE_REGSET("    AggAvg: Begin");

	dt_cg_node(dnp->dn_aggfun->dn_args, dlp, drp);

	DT_CG_AGG_IMPL(aid, sz, dlp, drp, dt_cg_agg_avg_impl,
		       dnp->dn_aggfun->dn_args->dn_reg);
	dt_regset_free(drp, dnp->dn_aggfun->dn_args->dn_reg);

	TRACE_REGSET("    AggAvg: End  ");
}

static void
dt_cg_agg_count_impl(dt_irlist_t *dlp, dt_regset_t *drp, int dreg)
{
	TRACE_REGSET("            Impl: Begin");

	/*
	 *	(*dreg)++;		// mov %r0, 1
	 *				// xadd [%rX + 0], %r0
	 */
	dt_regset_xalloc(drp, BPF_REG_0);
	emit(dlp, BPF_MOV_IMM(BPF_REG_0, 1));
	emit(dlp, BPF_XADD_REG(BPF_DW, dreg, 0, BPF_REG_0));
	dt_regset_free(drp, BPF_REG_0);

	TRACE_REGSET("            Impl: End  ");
}

static void
dt_cg_agg_count(dt_pcb_t *pcb, dt_ident_t *aid, dt_node_t *dnp,
		dt_irlist_t *dlp, dt_regset_t *drp)
{
	int	sz = 1 * sizeof(uint64_t);

	DT_CG_AGG_SET_STORAGE(aid, sz);

	TRACE_REGSET("    AggCnt: Begin");

	DT_CG_AGG_IMPL(aid, sz, dlp, drp, dt_cg_agg_count_impl);

	TRACE_REGSET("    AggCnt: End  ");
}

static void
dt_cg_agg_quantize_impl(dt_irlist_t *dlp, dt_regset_t *drp, int dreg, int vreg, int ireg, int maxbin)
{
	uint_t		L = dt_irlist_label(dlp);
	int		offreg;

	TRACE_REGSET("            Impl: Begin");

	if ((offreg = dt_regset_alloc(drp)) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	/* check bounds */
	emit(dlp,  BPF_BRANCH_IMM(BPF_JGT, vreg, maxbin, L));
	emit(dlp,  BPF_BRANCH_IMM(BPF_JLT, vreg, 0, L));

	/* *(dest + 8 * off) += incr */
	emit(dlp,  BPF_MOV_REG(offreg, vreg));
	emit(dlp,  BPF_ALU64_IMM(BPF_MUL, offreg, 8));
	emit(dlp,  BPF_ALU64_REG(BPF_ADD, offreg, dreg));
	emit(dlp,  BPF_XADD_REG(BPF_DW, offreg, 0, ireg));

	emitl(dlp, L,
		   BPF_NOP());
	dt_regset_free(drp, offreg);

	TRACE_REGSET("            Impl: End  ");
}

/*
 * Log-linear quantization is the most complicated of the *quantize()
 * aggregation functions.  This function quantizes the value held in
 * valreg to a 0-based bin number using the parameters factor, lmag,
 * hmag, and steps.
 *
 * First, let's illustrate log-linear based on an example.  We will
 * bin logarithmically for factor=10 -- that is, 0-9, 10-99, 100-999,
 * and so on.  Within each logarithmic range, we bin linearly.  E.g.,
 * for the logarithmic range 100-999, the bins are:
 *
 *    +-------+----------------+----------------------------------+
 *    |steps=5|    steps=10    |             steps=20             |
 *    +-------+----------------+----------------------------------+
 *    |100-199|         100-199|                  100-149  150-199|
 *    |200-399|200-299  300-399|200-249  250-299  300-349  350-399|
 *    |400-599|400-499  500-599|400-449  450-499  500-549  550-599|
 *    |600-799|600-699  700-799|600-649  650-699  700-749  750-799|
 *    |800-999|800-899  900-999|800-849  850-899  900-949  950-999|
 *    +-------+----------------+----------------------------------+
 *
 * Nominally, there are "steps" bins per logarithmic range, but values 0-99
 * do not actually appear here; they are not part of this logarithmic
 * range.  For steps=5, this makes the lowest bin narrower than the others;
 * for steps=10, it removes one bin; and for steps=20 two bins.  That is,
 * the actual number of bins for a logarithmic range is steps-steps/factor.
 *
 * For factor=10, the logarithmic range 10-99 is for mag=1, 100-999 for mag=2,
 * 1000-9999 for mag=3, and so on.  The parameters lmag and hmag indicate the
 * inclusive bounds for mag.
 *
 * So, all logarithmic ranges, put together, have
 *
 *     (hmag - lmag + 1) * (steps - steps/factor)
 *
 * bins.  We double that number to account for negative values.  We add
 * another 3 bins for:
 *
 *   - negative overflow
 *
 *   - underflow (values close to 0, whether negative or positive)
 *
 *   - positive overflow
 *
 * The bins are arranged in numerical order, starting with negative overflow,
 * ending with positive overflow, and with underflow exactly in the middle.
 */
static void
dt_cg_agg_llquantize_bin(dt_irlist_t *dlp, dt_regset_t *drp, int valreg,
			 int32_t factor, int32_t lmag, int32_t hmag,
			 int32_t steps)
{
	/*
	 * We say there are "steps" bins per logarithmic range,
	 * but steps/factor of them actually overlap with lower ranges.
	 */
	int steps_factor = steps / factor;

	/* the underflow bin is in the middle */
	int underflow_bin = 1 + (hmag - lmag + 1) * (steps - steps_factor);

	/* initial bucket_max */
	uint64_t bucket_max0 = powl(factor, lmag);

	/* registers */
	int indreg, magreg, tmpreg, maxreg;

	/* labels */
	uint_t		L1 = dt_irlist_label(dlp);
	uint_t		L2 = dt_irlist_label(dlp);
	uint_t		L3 = dt_irlist_label(dlp);
	uint_t		L4 = dt_irlist_label(dlp);
	uint_t		Lloop = dt_irlist_label(dlp);
	uint_t		Lbin = dt_irlist_label(dlp);
	uint_t		Lshift = dt_irlist_label(dlp);
	uint_t		Lend = dt_irlist_label(dlp);

	TRACE_REGSET("            Bin: Begin");

	if ((indreg = dt_regset_alloc(drp)) == -1 ||
	    (magreg = dt_regset_alloc(drp)) == -1 ||
	    (tmpreg = dt_regset_alloc(drp)) == -1 ||
	    (maxreg = dt_regset_alloc(drp)) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	/*
	 * %tmp = %val
	 * if (%val >= 0) goto L1
	 * %tmp *= -1
	 */
	emit(dlp,  BPF_MOV_REG(tmpreg, valreg));
	emit(dlp,  BPF_BRANCH_IMM(BPF_JSGE, valreg, 0, L1));
	emit(dlp,  BPF_ALU64_IMM(BPF_MUL, tmpreg, -1));

	/*
	 * check for "underflow" (smaller than the smallest bin)
	 * L1:
	 *     %mag = underflow_bin
	 *     %max = bucket_max0
	 *     if (%tmp < %max) goto Lend
	 */
	emitl(dlp, L1,
		   BPF_MOV_IMM(magreg, underflow_bin));
	dt_cg_xsetx(dlp, NULL, DT_LBL_NONE, maxreg, bucket_max0);
	emit(dlp,  BPF_BRANCH_REG(BPF_JLT, tmpreg, maxreg, Lend));

	/*
	 * loop over the logarithmic ranges
	 *     %mag = lmag                    lowest range
	 *     %ind = 0                       bin within this range
	 * Lloop:
	 *     %max *= factor;                increase max
	 *     if (%tmp < %max) goto Lbin;    found the right range
	 *     %mag++;                        otherwise, increase range
	 *     if (%mag > hmag) goto Lshift;  went beyond last range
	 *     goto Lloop;
	 */
	emit(dlp,  BPF_MOV_IMM(magreg, lmag));
	emit(dlp,  BPF_MOV_IMM(indreg, 0));
	emitl(dlp, Lloop,
		   BPF_ALU64_IMM(BPF_MUL, maxreg, factor));
	emit(dlp,  BPF_BRANCH_REG(BPF_JLT, tmpreg, maxreg, Lbin));
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, magreg, 1));
	emit(dlp,  BPF_BRANCH_IMM(BPF_JGT, magreg, hmag, Lshift));
	emit(dlp,  BPF_JUMP(Lloop));

	/*
	 * Found a bin
	 * Evaluate (%tmp*steps/%max) in a way that it will not overflow
	 * Lbin:
	 *     if (%mag != 0) %ind = %tmp / (%max / steps)
	 *     else           %ind = %tmp * steps / %max
	 */
	emitl(dlp, Lbin,
		   BPF_MOV_REG(indreg, tmpreg));
	emit(dlp,  BPF_BRANCH_IMM(BPF_JEQ, magreg, 0, L2));
	emit(dlp,  BPF_ALU64_IMM(BPF_DIV, maxreg, steps));
	emit(dlp,  BPF_JUMP(L3));
	emitl(dlp, L2,
		   BPF_ALU64_IMM(BPF_MUL, indreg, steps));
	emitl(dlp, L3,
		   BPF_ALU64_REG(BPF_DIV, indreg, maxreg));

	/*
	 * shift for low indices that can never happen
	 *     %ind -= steps_factor
	 */
	emit(dlp,  BPF_ALU64_IMM(BPF_SUB, indreg, steps_factor));

	/*
	 * shift to center around underflow_bin
	 * Lshift:
	 *     %mag = (%mag - lmag) * (steps - steps_factor) + %ind + 1
	 *     if (%v < 0) %mag *= -1
	 *     %mag += underflow_bin
	 */
	emitl(dlp, Lshift,
		   BPF_ALU64_IMM(BPF_SUB, magreg, lmag));
	emit(dlp,  BPF_ALU64_IMM(BPF_MUL, magreg, steps - steps_factor));
	emit(dlp,  BPF_ALU64_REG(BPF_ADD, magreg, indreg));
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, magreg, 1));
	emit(dlp,  BPF_BRANCH_IMM(BPF_JSGE, valreg, 0, L4));
	emit(dlp,  BPF_ALU64_IMM(BPF_MUL, magreg, -1));
	emitl(dlp, L4,
		   BPF_ALU64_IMM(BPF_ADD, magreg, underflow_bin));

	/*
	 * Lend:
	 *     %val = %mag
	 */
	emitl(dlp, Lend,
		   BPF_MOV_REG(valreg, magreg));

	dt_regset_free(drp, indreg);
	dt_regset_free(drp, magreg);
	dt_regset_free(drp, tmpreg);
	dt_regset_free(drp, maxreg);

	TRACE_REGSET("            Bin: End  ");
}

static void
dt_cg_agg_llquantize(dt_pcb_t *pcb, dt_ident_t *aid, dt_node_t *dnp,
		     dt_irlist_t *dlp, dt_regset_t *drp)
{
	/*
	 * For log linear quantization, we have four arguments in addition to
	 * the expression:
	 *
	 *    arg1 => Factor
	 *    arg2 => Lower magnitude
	 *    arg3 => Upper magnitude
	 *    arg4 => Steps per magnitude
	 */
	dt_node_t	*arg1 = dnp->dn_aggfun->dn_args->dn_list;
	dt_node_t	*arg2 = arg1->dn_list;
	dt_node_t	*arg3 = arg2->dn_list;
	dt_node_t	*arg4 = arg3->dn_list;
	dt_node_t	*incr;
	dt_idsig_t	*isp;
	uint64_t	factor, lmag, hmag, steps, arg, oarg;
	int		sz, ireg;

	if (arg1->dn_kind != DT_NODE_INT)
		dnerror(arg1, D_LLQUANT_FACTORTYPE, "llquantize( ) argument #1 "
			"must be an integer constant\n");

	factor = (uint64_t)arg1->dn_value;

	if (factor > UINT16_MAX || factor < 2)
		dnerror(arg1, D_LLQUANT_FACTORVAL, "llquantize( ) argument #1 "
			"must be an unsigned 16-bit quantity greater than or "
			"equal to 2\n");

	if (arg2->dn_kind != DT_NODE_INT)
		dnerror(arg2, D_LLQUANT_LMAGTYPE, "llquantize( ) argument #2 "
			"must be an integer constant\n");

	lmag = (uint64_t)arg2->dn_value;

	if (lmag > UINT16_MAX)
		dnerror(arg2, D_LLQUANT_LMAGVAL, "llquantize( ) argument #2 "
			"must be an unsigned 16-bit quantity\n");

	if (arg3->dn_kind != DT_NODE_INT)
		dnerror(arg3, D_LLQUANT_HMAGTYPE, "llquantize( ) argument #3 "
			"must be an integer constant\n");

	hmag = (uint64_t)arg3->dn_value;

	if (hmag > UINT16_MAX)
		dnerror(arg3, D_LLQUANT_HMAGVAL, "llquantize( ) argument #3 "
			"must be an unsigned 16-bit quantity\n");

	if (hmag < lmag)
		dnerror(arg3, D_LLQUANT_HMAGVAL, "llquantize( ) argument #3 "
			"(high magnitude) must be at least argument #2 (low "
			"magnitude)\n");

	if (powl(factor, hmag + 1) > (long double)UINT64_MAX)
		dnerror(arg3, D_LLQUANT_HMAGVAL, "llquantize( ) argument #3 "
			"(high magnitude) will cause overflow of 64 bits\n");

	if (!dt_node_is_posconst(arg4))
		dnerror(arg4, D_LLQUANT_STEPTYPE, "llquantize( ) argument #4 "
			"must be a non-zero positive integer constant\n");

	steps = (uint64_t)arg4->dn_value;

	if (steps > UINT16_MAX)
		dnerror(arg4, D_LLQUANT_STEPVAL, "llquantize( ) argument #4 "
			"must be an unsigned 16-bit quantity\n");

	if (steps < factor) {
		if (factor % steps != 0)
			dnerror(arg1, D_LLQUANT_STEPVAL, "llquantize() "
				"argument #4 (steps) must evenly divide "
				"argument #1 (factor) when steps<factor\n");
	}

	if (steps > factor) {
		if (steps % factor != 0)
			dnerror(arg1, D_LLQUANT_STEPVAL, "llquantize() "
				"argument #4 (steps) must be a multiple of "
				"argument #1 (factor) when steps>factor\n");

		if (lmag == 0) {
			if ((factor * factor) % steps != 0)
				dnerror(arg1, D_LLQUANT_STEPVAL,
					"llquantize() argument #4 (steps) must "
					"evenly divide the square of argument "
					"#1 (factor) when steps>factor and "
					"lmag==0\n");
		} else {
			unsigned long long	ii = powl(factor, lmag + 1);

			if (ii % steps != 0)
				dnerror(arg1, D_LLQUANT_STEPVAL,
					"llquantize() argument #4 (steps) must "
					"evenly divide pow(argument #1 "
					"(factor), lmag (argument #2) + 1) "
					"when steps>factor and lmag>0\n");
		}
	}

	arg = (steps << DTRACE_LLQUANTIZE_STEPSSHIFT) |
	      (hmag << DTRACE_LLQUANTIZE_HMAGSHIFT) |
	      (lmag << DTRACE_LLQUANTIZE_LMAGSHIFT) |
	      (factor << DTRACE_LLQUANTIZE_FACTORSHIFT);

	assert(arg != 0);

	isp = (dt_idsig_t *)aid->di_data;

	oarg = isp->dis_auxinfo;
	if (oarg == 0) {
		/*
		 * This is the first time we've seen an llquantize() for this
		 * aggregation; we'll store our argument as the auxiliary
		 * signature information.
		 */
		isp->dis_auxinfo = arg;
	} else if (oarg != arg) {
		/*
		 * If we have seen this llquantize() before and the argument
		 * doesn't match the original argument, pick the original
		 * argument apart to concisely report the mismatch.
		 */
		int	ofactor = DTRACE_LLQUANTIZE_FACTOR(oarg);
		int	olmag = DTRACE_LLQUANTIZE_LMAG(oarg);
		int	ohmag = DTRACE_LLQUANTIZE_HMAG(oarg);
		int	osteps = DTRACE_LLQUANTIZE_STEPS(oarg);

		if (ofactor != factor)
			dnerror(dnp, D_LLQUANT_MATCHFACTOR, "llquantize() "
				"factor (argument #1) doesn't match previous "
				"declaration: expected %d, found %d\n",
				ofactor, (int)factor);

		if (olmag != lmag)
			dnerror(dnp, D_LLQUANT_MATCHLMAG, "llquantize() lmag "
				"(argument #2) doesn't match previous "
				"declaration: expected %d, found %d\n",
				olmag, (int)lmag);

		if (ohmag != hmag)
			dnerror(dnp, D_LLQUANT_MATCHHMAG, "llquantize() hmag "
				"(argument #3) doesn't match previous "
				"declaration: expected %d, found %d\n",
				ohmag, (int)hmag);

		if (osteps != steps)
			dnerror(dnp, D_LLQUANT_MATCHSTEPS, "llquantize() steps "
				"(argument #4) doesn't match previous "
				"declaration: expected %d, found %d\n",
				osteps, (int)steps);

		/*
		 * We shouldn't be able to get here -- one of the parameters
		 * must be mismatched if the arguments didn't match.
		 */
		assert(0);
	}

	incr = dt_cg_agg_opt_incr(dnp, arg4, "llquantize", 6);

	sz = (hmag - lmag + 1) * (steps - steps / factor) * 2 + 3;
	sz *= sizeof(uint64_t);

	DT_CG_AGG_SET_STORAGE(aid, sz);

	TRACE_REGSET("    AggLlq: Begin");

	dt_cg_node(dnp->dn_aggfun->dn_args, dlp, drp);
	dt_cg_agg_llquantize_bin(dlp, drp, dnp->dn_aggfun->dn_args->dn_reg,
	    factor, lmag, hmag, steps);

	if (incr == NULL) {
		if ((ireg = dt_regset_alloc(drp)) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
		emit(dlp, BPF_MOV_IMM(ireg, 1));
	} else {
		dt_cg_node(incr, dlp, drp);
		ireg = incr->dn_reg;
	}

	DT_CG_AGG_IMPL(aid, sz, dlp, drp, dt_cg_agg_quantize_impl,
		       dnp->dn_aggfun->dn_args->dn_reg, ireg,
		       (hmag - lmag + 1) * (steps - steps / factor) * 2 + 2);

	dt_regset_free(drp, dnp->dn_aggfun->dn_args->dn_reg);
	dt_regset_free(drp, ireg);

	TRACE_REGSET("    AggLlq: End  ");
}

static void
dt_cg_agg_lquantize(dt_pcb_t *pcb, dt_ident_t *aid, dt_node_t *dnp,
		    dt_irlist_t *dlp, dt_regset_t *drp)
{
	/*
	 * For linear quantization, we have between two and four arguments in
	 * addition to the expression:
	 *
	 *    arg1 => Base value
	 *    arg2 => Limit value
	 *    arg3 => Quantization level step size (defaults to 1)
	 *    arg4 => Quantization increment value (defaults to 1)
	 */
	dt_node_t	*arg1 = dnp->dn_aggfun->dn_args->dn_list;
	dt_node_t	*arg2 = arg1->dn_list;
	dt_node_t	*arg3 = arg2->dn_list;
	dt_node_t	*incr;
	dt_idsig_t	*isp;
	uint64_t	nlevels, arg, oarg;
	uint64_t	step = 1;
	int64_t		baseval, limitval;
	int		sz, ireg;
	dt_ident_t	*idp;

	if (arg1->dn_kind != DT_NODE_INT)
		dnerror(arg1, D_LQUANT_BASETYPE, "lquantize( ) argument #1 "
			"must be an integer constant\n");

	baseval = (int64_t)arg1->dn_value;

	if (baseval < INT32_MIN || baseval > INT32_MAX)
		dnerror(arg1, D_LQUANT_BASEVAL, "lquantize( ) argument #1 must "
			"be a 32-bit quantity\n");

	if (arg2->dn_kind != DT_NODE_INT)
		dnerror(arg2, D_LQUANT_LIMTYPE, "lquantize( ) argument #2 must "
			"be an integer constant\n");

	limitval = (int64_t)arg2->dn_value;

	if (limitval < INT32_MIN || limitval > INT32_MAX)
		dnerror(arg2, D_LQUANT_LIMVAL, "lquantize( ) argument #2 must "
			"be a 32-bit quantity\n");

	if (limitval <= baseval)
		dnerror(dnp, D_LQUANT_MISMATCH,
			"lquantize( ) base (argument #1) must be less than "
			"limit (argument #2)\n");

	if (arg3 != NULL) {
		if (!dt_node_is_posconst(arg3))
			dnerror(arg3, D_LQUANT_STEPTYPE, "lquantize( ) "
				"argument #3 must be a non-zero positive "
				"integer constant\n");

		step = arg3->dn_value;
		if (step > UINT16_MAX)
			dnerror(arg3, D_LQUANT_STEPVAL, "lquantize( ) "
				"argument #3 must be a 16-bit quantity\n");
	}

	nlevels = (limitval - baseval) / step;

	if (nlevels == 0)
		dnerror(dnp, D_LQUANT_STEPLARGE,
			"lquantize( ) step (argument #3) too large: must have "
			"at least one quantization level\n");

	if (nlevels > UINT16_MAX)
		dnerror(dnp, D_LQUANT_STEPSMALL, "lquantize( ) step (argument "
			"#3) too small: number of quantization levels must be "
			"a 16-bit quantity\n");

	arg = (step << DTRACE_LQUANTIZE_STEPSHIFT) |
	      (nlevels << DTRACE_LQUANTIZE_LEVELSHIFT) |
	      ((baseval << DTRACE_LQUANTIZE_BASESHIFT) &
	       DTRACE_LQUANTIZE_BASEMASK);

	assert(arg != 0);

	isp = (dt_idsig_t *)aid->di_data;

	oarg = isp->dis_auxinfo;
	if (oarg == 0) {
		/*
		 * This is the first time we've seen an lquantize() for this
		 * aggregation; we'll store our argument as the auxiliary
		 * signature information.
		 */
		isp->dis_auxinfo = arg;
	} else if (oarg != arg) {
		/*
		 * If we have seen this lquantize() before and the argument
		 * doesn't match the original argument, pick the original
		 * argument apart to concisely report the mismatch.
		 */
		int	obaseval = DTRACE_LQUANTIZE_BASE(oarg);
		int	onlevels = DTRACE_LQUANTIZE_LEVELS(oarg);
		int	ostep = DTRACE_LQUANTIZE_STEP(oarg);

		if (obaseval != baseval)
			dnerror(dnp, D_LQUANT_MATCHBASE, "lquantize( ) base "
				"(argument #1) doesn't match previous "
				"declaration: expected %d, found %d\n",
				obaseval, (int)baseval);

		if (onlevels * ostep != nlevels * step)
			dnerror(dnp, D_LQUANT_MATCHLIM, "lquantize( ) limit "
				"(argument #2) doesn't match previous "
				"declaration: expected %d, found %d\n",
				obaseval + onlevels * ostep,
				(int)baseval + (int)nlevels * (int)step);

		if (ostep != step)
			dnerror(dnp, D_LQUANT_MATCHSTEP, "lquantize( ) step "
				"(argument #3) doesn't match previous "
				"declaration: expected %d, found %d\n",
				ostep, (int)step);

		/*
		 * We shouldn't be able to get here -- one of the parameters
		 * must be mismatched if the arguments didn't match.
		 */
		assert(0);
	}

	incr = dt_cg_agg_opt_incr(dnp, arg3, "lquantize", 5);

	sz = nlevels + 2;
	sz *= sizeof(uint64_t);

	DT_CG_AGG_SET_STORAGE(aid, sz);

	TRACE_REGSET("    AggLq : Begin");

	dt_cg_node(dnp->dn_aggfun->dn_args, dlp, drp);

	/* quantize the value to a 0-based bin # using dt_agg_lqbin() */
	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	emit(dlp,  BPF_MOV_REG(BPF_REG_1, dnp->dn_aggfun->dn_args->dn_reg));
	emit(dlp,  BPF_MOV_IMM(BPF_REG_2, baseval));
	emit(dlp,  BPF_MOV_IMM(BPF_REG_3, nlevels));
	emit(dlp,  BPF_MOV_IMM(BPF_REG_4, step));
	idp = dt_dlib_get_func(yypcb->pcb_hdl, "dt_agg_lqbin");
	assert(idp != NULL);
	dt_regset_xalloc(drp, BPF_REG_0);
	emite(dlp, BPF_CALL_FUNC(idp->di_id), idp);
	dt_regset_free_args(drp);
	emit(dlp,  BPF_MOV_REG(dnp->dn_aggfun->dn_args->dn_reg, BPF_REG_0));
	dt_regset_free(drp, BPF_REG_0);

	if (incr == NULL) {
		if ((ireg = dt_regset_alloc(drp)) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

		emit(dlp, BPF_MOV_IMM(ireg, 1));
	} else {
		dt_cg_node(incr, dlp, drp);
		ireg = incr->dn_reg;
	}

	DT_CG_AGG_IMPL(aid, sz, dlp, drp, dt_cg_agg_quantize_impl,
		       dnp->dn_aggfun->dn_args->dn_reg, ireg, nlevels + 1);

	dt_regset_free(drp, dnp->dn_aggfun->dn_args->dn_reg);
	dt_regset_free(drp, ireg);

	TRACE_REGSET("    AggLq : End  ");
}

static void
dt_cg_agg_max_impl(dt_irlist_t *dlp, dt_regset_t *drp, int dreg, int vreg)
{
	int	treg;			/* temporary value register */
	uint_t	Lmax = dt_irlist_label(dlp);

	TRACE_REGSET("            Impl: Begin");

	if ((treg = dt_regset_alloc(drp)) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	/* %treg = *(%dreg) */
	emit(dlp,  BPF_LOAD(BPF_DW, treg, dreg, 0));
	/* if (%treg >= %vreg) goto Lmax */
	emit(dlp,  BPF_BRANCH_REG(BPF_JSGE, treg, vreg, Lmax));
	/* *(%dreg) = %vreg */
	emit(dlp,  BPF_STORE(BPF_DW, dreg, 0, vreg));
	emitl(dlp, Lmax,
		   BPF_NOP());
	dt_regset_free(drp, treg);

	TRACE_REGSET("            Impl: End  ");
}

static void
dt_cg_agg_max(dt_pcb_t *pcb, dt_ident_t *aid, dt_node_t *dnp,
	      dt_irlist_t *dlp, dt_regset_t *drp)
{
	int	sz = 1 * sizeof(uint64_t);

	DT_CG_AGG_SET_STORAGE(aid, sz);

	TRACE_REGSET("    AggMax: Begin");

	dt_cg_node(dnp->dn_aggfun->dn_args, dlp, drp);
	DT_CG_AGG_IMPL(aid, sz, dlp, drp, dt_cg_agg_max_impl,
		       dnp->dn_aggfun->dn_args->dn_reg);
	dt_regset_free(drp, dnp->dn_aggfun->dn_args->dn_reg);

	TRACE_REGSET("    AggMax: End  ");

}

static void
dt_cg_agg_min_impl(dt_irlist_t *dlp, dt_regset_t *drp, int dreg, int vreg)
{
	int	treg;			/* temporary value register */
	uint_t	Lmin = dt_irlist_label(dlp);

	TRACE_REGSET("            Impl: Begin");

	if ((treg = dt_regset_alloc(drp)) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	/* %treg = *(%dreg) */
	emit(dlp,  BPF_LOAD(BPF_DW, treg, dreg, 0));
	/* if (%treg <= %vreg) goto Lmin */
	emit(dlp,  BPF_BRANCH_REG(BPF_JSLE, treg, vreg, Lmin));
	/* *(%dreg) = %vreg */
	emit(dlp,  BPF_STORE(BPF_DW, dreg, 0, vreg));
	emitl(dlp, Lmin,
		   BPF_NOP());
	dt_regset_free(drp, treg);

	TRACE_REGSET("            Impl: End  ");
}

static void
dt_cg_agg_min(dt_pcb_t *pcb, dt_ident_t *aid, dt_node_t *dnp,
	      dt_irlist_t *dlp, dt_regset_t *drp)
{
	int	sz = 1 * sizeof(uint64_t);

	DT_CG_AGG_SET_STORAGE(aid, sz);

	TRACE_REGSET("    AggMin: Begin");

	dt_cg_node(dnp->dn_aggfun->dn_args, dlp, drp);
	DT_CG_AGG_IMPL(aid, sz, dlp, drp, dt_cg_agg_min_impl,
		       dnp->dn_aggfun->dn_args->dn_reg);
	dt_regset_free(drp, dnp->dn_aggfun->dn_args->dn_reg);

	TRACE_REGSET("    AggMin: End  ");
}

static void
dt_cg_agg_quantize(dt_pcb_t *pcb, dt_ident_t *aid, dt_node_t *dnp,
		   dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_ident_t	*idp;
	dt_node_t	*incr;
	int		ireg, sz = DTRACE_QUANTIZE_NBUCKETS * sizeof(uint64_t);

	/*
	 * The quantize() implementation is currently hardwired for
	 *     DTRACE_QUANTIZE_NBUCKETS 127
	 *     DTRACE_QUANTIZE_ZEROBUCKET 63
	 * These values are defined in include/dtrace/actions_defines.h
	 */
	assert(DTRACE_QUANTIZE_NBUCKETS == 127 &&
	    DTRACE_QUANTIZE_ZEROBUCKET == 63);

	incr = dt_cg_agg_opt_incr(dnp, dnp->dn_aggfun->dn_args, "quantize", 2);

	DT_CG_AGG_SET_STORAGE(aid, sz);

	TRACE_REGSET("    AggQ  : Begin");

	dt_cg_node(dnp->dn_aggfun->dn_args, dlp, drp);

	/* quantize the value to a 0-based bin # using dt_agg_qbin() */
	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	emit(dlp,  BPF_MOV_REG(BPF_REG_1, dnp->dn_aggfun->dn_args->dn_reg));
	idp = dt_dlib_get_func(yypcb->pcb_hdl, "dt_agg_qbin");
	assert(idp != NULL);
	dt_regset_xalloc(drp, BPF_REG_0);
	emite(dlp, BPF_CALL_FUNC(idp->di_id), idp);
	dt_regset_free_args(drp);
	emit(dlp,  BPF_MOV_REG(dnp->dn_aggfun->dn_args->dn_reg, BPF_REG_0));
	dt_regset_free(drp, BPF_REG_0);

	if (incr == NULL) {
		if ((ireg = dt_regset_alloc(drp)) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

		emit(dlp, BPF_MOV_IMM(ireg, 1));
	} else {
		dt_cg_node(incr, dlp, drp);
		ireg = incr->dn_reg;
	}

	DT_CG_AGG_IMPL(aid, sz, dlp, drp, dt_cg_agg_quantize_impl,
		       dnp->dn_aggfun->dn_args->dn_reg, ireg,
		       DTRACE_QUANTIZE_NBUCKETS - 1);

	dt_regset_free(drp, dnp->dn_aggfun->dn_args->dn_reg);
	dt_regset_free(drp, ireg);

	TRACE_REGSET("    AggQ  : End  ");
}

static void
dt_cg_agg_stddev_impl(dt_irlist_t *dlp, dt_regset_t *drp, int dreg, int vreg,
		      uint64_t hreg, uint64_t lreg)
{
	int	onereg, lowreg;
	uint_t	Lncr = dt_irlist_label(dlp);

	TRACE_REGSET("            Impl: Begin");

	/* Get some registers to work with */
	if ((onereg = dt_regset_alloc(drp)) == -1 ||
	    (lowreg = dt_regset_alloc(drp)) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	/* Set onereg to a value of 1 for repeated use */
	emit(dlp,  BPF_MOV_IMM(onereg, 1));

	/* dst[0]++; increment count of calls eq. value count */
	emit(dlp,  BPF_XADD_REG(BPF_DW, dreg, 0, onereg));

	/* dst[1] += val; add value to the total sum */
	emit(dlp,  BPF_XADD_REG(BPF_DW, dreg, 8, vreg));

	/* Add double-dst[2-3] += sq(val) starting here */

	/* Load the lower half (64 bits) of the previous established value */
	emit(dlp,  BPF_LOAD(BPF_DW, lowreg, dreg, 16));

	/* Add low part of the value; *lowreg += *lreg */
	emit(dlp,  BPF_ALU64_REG(BPF_ADD, lowreg, lreg));

	/* Handle the overflow/carry case; overflow if lowreg < lreg */
	emit(dlp,  BPF_BRANCH_REG(BPF_JLE, lreg, lowreg, Lncr));
	emit(dlp,  BPF_XADD_REG(BPF_DW, dreg, 24, onereg)) /* Carry */;
	emitl(dlp, Lncr,
		   BPF_STORE(BPF_DW, dreg, 16, lowreg));

	emit(dlp,  BPF_XADD_REG(BPF_DW, dreg, 24, hreg)); /* Add higher half */

	dt_regset_free(drp, onereg);
	dt_regset_free(drp, lowreg);

	TRACE_REGSET("            Impl: End  ");
}


static void
dt_cg_agg_stddev(dt_pcb_t *pcb, dt_ident_t *aid, dt_node_t *dnp,
		 dt_irlist_t *dlp, dt_regset_t *drp)
{
	int	sz = 4 * sizeof(uint64_t);
	uint64_t  hi_reg, lowreg, midreg, lmdreg;

	/* labels */
	uint_t  Lpos = dt_irlist_label(dlp);
	uint_t  Lncy = dt_irlist_label(dlp);

	DT_CG_AGG_SET_STORAGE(aid, sz);

	TRACE_REGSET("    AggSDv: Begin");

	dt_cg_node(dnp->dn_aggfun->dn_args, dlp, drp);

	/* Handle sq(val) starting here; 128-bit answer in 2 64-bit registers */
	/* val = (uint64_t)dnp->dn_aggfun->dn_args->dn_value; */
	/* sqr = (__int128)val * val; */
	/* low = sqr & 0xFFFFFFFFFFFFFFFFULL; */
	/* hi_ = sqr >> 64; */
	/* get some registers to work with */
	if ((hi_reg = dt_regset_alloc(drp)) == -1 ||
	    (lowreg = dt_regset_alloc(drp)) == -1 ||
	    (midreg = dt_regset_alloc(drp)) == -1 ||
	    (lmdreg = dt_regset_alloc(drp)) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	/* If value is negative make it positive, sq() is positive anyhow */
	emit(dlp,  BPF_MOV_REG(lowreg, dnp->dn_aggfun->dn_args->dn_reg));
	emit(dlp,  BPF_BRANCH_IMM(BPF_JSGE, lowreg, 0, Lpos));
	emit(dlp,  BPF_NEG_REG(lowreg))  /* Change to a positive value */;

	/* Now we are sure lowreg holds our absolute value */
	/* Split our value into two pieces: high and low */
	emitl(dlp, Lpos,
		   BPF_MOV_REG(hi_reg, lowreg));

	/* Put least significant half in lowreg */
	/* Implicit AND of lowreg with 0xFFFFFFFF mask */
	emit(dlp,  BPF_ALU64_IMM(BPF_LSH, lowreg, 32));
	emit(dlp,  BPF_ALU64_IMM(BPF_RSH, lowreg, 32));
	/* Put most significant half in (the lower part of) hi_reg */
	emit(dlp,  BPF_ALU64_IMM(BPF_RSH, hi_reg, 32));

	/* Multiply using similar approach to algebraic FOIL of binomial */
	/* That is: high*high + 2*high*low + low*low */
	/* Contain interim values to 64 bits or account for carry */

	/* First multiply high times low value */
	/* Value must be doubled for answer, eventually */
	/* This result represents a value shifted 32 bits to the right */
	emit(dlp,  BPF_MOV_REG(midreg, lowreg));
	emit(dlp,  BPF_ALU64_REG(BPF_MUL, midreg, hi_reg));
	/* Product of low values */
	emit(dlp,  BPF_ALU64_REG(BPF_MUL, lowreg, lowreg));
	/* Product of high values; result represented shifted 64 to the right */
	emit(dlp,  BPF_ALU64_REG(BPF_MUL, hi_reg, hi_reg));

	/* Now add the pieces in their proper place */

	/* Get the bottom half of the mid value to add to the low value */
	emit(dlp,  BPF_MOV_REG(lmdreg, midreg));
	/* The mid values are doubled, as O and I in FOIL are for a square */
	/* Adjust for shifted representation with respect to low half value */
	/* Implicit AND of lmdreg (lower mid) with 0xFFFFFFFF mask, times 2 */
	emit(dlp,  BPF_ALU64_IMM(BPF_LSH, lmdreg, 33));
	/* Upper half times 2 + carry bit from lower half x2 */
	emit(dlp,  BPF_ALU64_IMM(BPF_RSH, midreg, 31));

	/* Add low value part from mid to lowreg */
	emit(dlp,  BPF_ALU64_REG(BPF_ADD, lowreg, lmdreg));
	/* Handle the overflow/carry case */
	emit(dlp,  BPF_BRANCH_REG(BPF_JLT, lmdreg, lowreg, Lncy));
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, hi_reg, 1)) /* account for carry */;

	/* Sum high value; no overflow expected nor accounted for */
	emitl(dlp, Lncy,
		   BPF_ALU64_REG(BPF_ADD, hi_reg, midreg));

	dt_regset_free(drp, lmdreg);
	dt_regset_free(drp, midreg);

	DT_CG_AGG_IMPL(aid, sz, dlp, drp, dt_cg_agg_stddev_impl,
		       dnp->dn_aggfun->dn_args->dn_reg, hi_reg, lowreg);

	dt_regset_free(drp, dnp->dn_aggfun->dn_args->dn_reg);
	dt_regset_free(drp, hi_reg);
	dt_regset_free(drp, lowreg);

	TRACE_REGSET("    AggSDv: End  ");
}

static void
dt_cg_agg_sum_impl(dt_irlist_t *dlp, dt_regset_t *drp, int dreg, int vreg)
{
	TRACE_REGSET("            Impl: Begin");

	/* *(%dreg) += %vreg */
	emit(dlp, BPF_XADD_REG(BPF_DW, dreg, 0, vreg));

	TRACE_REGSET("            Impl: End  ");
}

static void
dt_cg_agg_sum(dt_pcb_t *pcb, dt_ident_t *aid, dt_node_t *dnp,
	      dt_irlist_t *dlp, dt_regset_t *drp)
{
	int	sz = 1 * sizeof(uint64_t);

	DT_CG_AGG_SET_STORAGE(aid, sz);

	TRACE_REGSET("    AggSum: Begin");

	dt_cg_node(dnp->dn_aggfun->dn_args, dlp, drp);
	DT_CG_AGG_IMPL(aid, sz, dlp, drp, dt_cg_agg_sum_impl,
		       dnp->dn_aggfun->dn_args->dn_reg);
	dt_regset_free(drp, dnp->dn_aggfun->dn_args->dn_reg);

	TRACE_REGSET("    AggSum: End  ");
}

typedef void dt_cg_aggfunc_f(dt_pcb_t *, dt_ident_t *, dt_node_t *,
			     dt_irlist_t *, dt_regset_t *);

static dt_cg_aggfunc_f *_dt_cg_agg[DT_AGG_NUM] = {
	[DT_AGG_IDX(DT_AGG_AVG)]	= &dt_cg_agg_avg,
	[DT_AGG_IDX(DT_AGG_COUNT)]	= &dt_cg_agg_count,
	[DT_AGG_IDX(DT_AGG_LLQUANTIZE)]	= &dt_cg_agg_llquantize,
	[DT_AGG_IDX(DT_AGG_LQUANTIZE)]	= &dt_cg_agg_lquantize,
	[DT_AGG_IDX(DT_AGG_MAX)]	= &dt_cg_agg_max,
	[DT_AGG_IDX(DT_AGG_MIN)]	= &dt_cg_agg_min,
	[DT_AGG_IDX(DT_AGG_QUANTIZE)]	= &dt_cg_agg_quantize,
	[DT_AGG_IDX(DT_AGG_STDDEV)]	= &dt_cg_agg_stddev,
	[DT_AGG_IDX(DT_AGG_SUM)]	= &dt_cg_agg_sum,
};

static void
dt_cg_agg(dt_pcb_t *pcb, dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_ident_t	*aid, *fid;
	dt_cg_aggfunc_f	*aggfp;

	/*
	 * If the aggregation has no aggregating function applied to it, then
	 * this statement has no effect.  Flag this as a programming error.
	 */
	if (dnp->dn_aggfun == NULL)
		dnerror(dnp, D_AGG_NULL, "expression has null effect: @%s\n",
			dnp->dn_ident->di_name);

	aid = dnp->dn_ident;
	fid = dnp->dn_aggfun->dn_ident;

	if (dnp->dn_aggfun->dn_args != NULL &&
	    dt_node_is_scalar(dnp->dn_aggfun->dn_args) == 0)
		dnerror(dnp->dn_aggfun, D_AGG_SCALAR, "%s( ) argument #1 must "
			"be of scalar type\n", fid->di_name);

	/* FIXME */
	if (dnp->dn_aggtup != NULL)
		dnerror(dnp->dn_aggtup, D_ARR_BADREF, "indexing is not "
			"supported yet: @%s\n", dnp->dn_ident->di_name);


	assert(fid->di_id >= DT_AGG_BASE && fid->di_id < DT_AGG_HIGHEST);

	dt_cg_clsflags(pcb, DTRACEACT_AGGREGATION, dnp);
	aggfp = _dt_cg_agg[DT_AGG_IDX(fid->di_id)];
	assert(aggfp != NULL);

	(*aggfp)(pcb, aid, dnp, dlp, drp);
	dt_aggid_add(pcb->pcb_hdl, aid);
}

void
dt_cg(dt_pcb_t *pcb, dt_node_t *dnp)
{
	dt_xlator_t	*dxp = NULL;
	dt_node_t	*act;

	if (pcb->pcb_regs == NULL) {
		pcb->pcb_regs = dt_regset_create(
					pcb->pcb_hdl->dt_conf.dtc_difintregs,
					dt_cg_spill_store, dt_cg_spill_load);
		if (pcb->pcb_regs == NULL)
			longjmp(pcb->pcb_jmpbuf, EDT_NOMEM);
	}

	dt_regset_reset(pcb->pcb_regs);

	dt_irlist_destroy(&pcb->pcb_ir);
	dt_irlist_create(&pcb->pcb_ir);
	pcb->pcb_exitlbl = dt_irlist_label(&pcb->pcb_ir);

	pcb->pcb_bufoff = 0;

	if (dt_node_is_dynamic(dnp))
		dnerror(dnp, D_CG_DYN, "expression cannot evaluate to result "
			"of dynamic type\n");

	/*
	 * If we're generating code for a translator body, assign the input
	 * parameter to the first available register (i.e. caller passes %r1).
	 */
	if (dnp->dn_kind == DT_NODE_MEMBER) {
		dxp = dnp->dn_membxlator;
		dnp = dnp->dn_membexpr;

		dxp->dx_ident->di_flags |= DT_IDFLG_CGREG;
		dxp->dx_ident->di_id = dt_regset_alloc(pcb->pcb_regs);
		dt_cg_node(dnp, &pcb->pcb_ir, pcb->pcb_regs);
	} else if (dnp->dn_kind == DT_NODE_CLAUSE) {
		dt_cg_prologue(pcb, dnp->dn_pred);

		for (act = dnp->dn_acts; act != NULL; act = act->dn_list) {
			switch (act->dn_kind) {
			case DT_NODE_DFUNC: {
				const dt_cg_actdesc_t	*actdp;
				dt_ident_t		*idp;

				idp = act->dn_expr->dn_ident;
				actdp = &_dt_cg_actions[DT_ACT_IDX(idp->di_id)];
				if (actdp->fun) {
					dt_cg_clsflags(pcb, actdp->kind, act);
					actdp->fun(pcb, act->dn_expr,
						   actdp->kind);
				}
				break;
			}
			case DT_NODE_AGG:
				dt_cg_agg(pcb, act, &pcb->pcb_ir,
					  pcb->pcb_regs);
				break;
			case DT_NODE_DEXPR:
				if (act->dn_expr->dn_kind == DT_NODE_AGG)
					dt_cg_agg(pcb, act->dn_expr,
						  &pcb->pcb_ir, pcb->pcb_regs);
				else
					dt_cg_node(act->dn_expr, &pcb->pcb_ir,
						   pcb->pcb_regs);

				if (act->dn_expr->dn_reg != -1)
					dt_regset_free(pcb->pcb_regs,
						       act->dn_expr->dn_reg);
				break;
			default:
				dnerror(dnp, D_UNKNOWN, "internal error -- "
					"node kind %u is not a valid "
					"statement\n", dnp->dn_kind);
			}
		}
		if (dnp->dn_acts == NULL)
			pcb->pcb_stmt->dtsd_clauseflags |= DT_CLSFLAG_DATAREC;

		dt_cg_epilogue(pcb);
	} else if (dnp->dn_kind == DT_NODE_TRAMPOLINE) {
		assert(pcb->pcb_probe != NULL);

		pcb->pcb_probe->prov->impl->trampoline(pcb);
	} else
		dt_cg_node(dnp, &pcb->pcb_ir, pcb->pcb_regs);

	if (dnp->dn_kind == DT_NODE_MEMBER) {
		dt_regset_free(pcb->pcb_regs, dxp->dx_ident->di_id);
		dxp->dx_ident->di_id = 0;
		dxp->dx_ident->di_flags &= ~DT_IDFLG_CGREG;
	}
}
