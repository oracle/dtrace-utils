/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <sys/types.h>

#include <stdlib.h>
#include <setjmp.h>
#include <assert.h>
#include <errno.h>

#include <dt_impl.h>
#include <dt_grammar.h>
#include <dt_parser.h>
#include <dt_printf.h>
#include <dt_provider.h>
#include <dt_probe.h>
#include <dt_bpf_builtins.h>
#include <bpf_asm.h>

static void dt_cg_xsetx(dt_irlist_t *, dt_ident_t *, uint_t, int, uint64_t);
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
 * The caller should NOT depend on any register values that exist at the end of
 * the trampiline prologue.
 */
void
dt_cg_tramp_prologue(dt_pcb_t *pcb, uint_t lbl_exit)
{
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	dt_ident_t	*mem = dt_dlib_get_map(pcb->pcb_hdl, "mem");
	struct bpf_insn	instr;

	assert(mem != NULL);

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
	 *	dctx.ctx = ctx;		// stdw [%fp + DCTX_FP(DCTX_CTX)], %r1
	 */
	instr = BPF_STORE(BPF_DW, BPF_REG_FP, DCTX_FP(DCTX_CTX), BPF_REG_1);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

	/*
	 *	key = 0;                // stw [%fp + DCTX_FP(DCTX_MST)], 0
	 *	rc = bpf_map_lookup_elem(mem, &key);
	 *				// lddw %r1, &mem
	 *				// mov %r2, %fp
	 *				// add %r2, DT_STK_LVA_BASE
	 *				// call bpf_map_lookup_elem
	 *				//     (%r1 ... %r5 clobbered)
	 *				//     (%r0 = 'mem' BPF map value)
	 *	if (rc == 0)            // jeq %r0, 0, lbl_exit
	 *		goto exit;
	 *				//     (%r0 = pointer to dt_mstate_t)
	 *	dctx.mst = rc;          // stdw [%fp + DCTX_FP(DCTX_MST)], %r0
	 */
	instr = BPF_STORE_IMM(BPF_W, BPF_REG_FP, DCTX_FP(DCTX_MST), 0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	dt_cg_xsetx(dlp, mem, DT_LBL_NONE, BPF_REG_1, mem->di_id);
	instr = BPF_MOV_REG(BPF_REG_2, BPF_REG_FP);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, DCTX_FP(DCTX_MST));
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_CALL_HELPER(BPF_FUNC_map_lookup_elem);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_0, 0, lbl_exit);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE(BPF_DW, BPF_REG_FP, DCTX_FP(DCTX_MST), BPF_REG_0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

	/*
	 *      buf = rc + roundup(sizeof(dt_mstate_t), 8);
	 *				// add %r0, roundup(
	 *						sizeof(dt_mstate_t), 8)
	 *      *((uint64_t *)&buf[0]) = 0;
	 *				// stdw [%r0 + 0], 0
	 *      buf += 8;		// add %r0, 8
	 *				//     (%r0 = pointer to buffer space)
	 *      dctx.buf = buf;		// stdw [%fp + DCTX_FP(DCTX_BUF)], %r0
	 */
	instr = BPF_ALU64_IMM(BPF_ADD, BPF_REG_0,
			      roundup(sizeof(dt_mstate_t), 8));
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE_IMM(BPF_DW, BPF_REG_0, 0, 0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_ALU64_IMM(BPF_ADD, BPF_REG_0, 8);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE(BPF_DW, BPF_REG_FP, DCTX_FP(DCTX_BUF), BPF_REG_0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
}

static int
dt_cg_call_clause(dtrace_hdl_t *dtp, dt_ident_t *idp, dt_irlist_t *dlp)
{
	struct bpf_insn	instr;

	instr = BPF_MOV_REG(BPF_REG_1, BPF_REG_FP);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, DCTX_FP(0));
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_CALL_FUNC(idp->di_id);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	dlp->dl_last->di_extern = idp;

	return 0;
}

void
dt_cg_tramp_epilogue(dt_pcb_t *pcb, uint_t lbl_exit)
{
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	struct bpf_insn	instr;

	/*
	 *	dt_program(dctx);	// mov %r1, %fp
	 *				// add %fp, DCTX_FP(0)
	 *				// call dt_program
	 *	(repeated for each clause)
	 */
	dt_probe_clause_iter(pcb->pcb_hdl, pcb->pcb_probe,
			     (dt_clause_f *)dt_cg_call_clause, dlp);

	/*
	 * exit:
	 *	return 0;		// mov %r0, 0
	 *				// exit
	 * }
	 */
	instr = BPF_MOV_IMM(BPF_REG_0, 0);
	dt_irlist_append(dlp, dt_cg_node_alloc(lbl_exit, instr));
	instr = BPF_RETURN();
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
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
	dt_ident_t	*prid = dt_dlib_get_var(pcb->pcb_hdl, "PRID");
	struct bpf_insn	instr;

	assert(epid != NULL);
	assert(prid != NULL);

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
	instr = BPF_STORE(BPF_DW, BPF_REG_FP, DT_STK_DCTX, BPF_REG_1);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

	/*
	 *	buf = dctx->buf;	// lddw %r0, [%fp + DT_STK_DCTX]
	 *				// lddw %r9, [%r0 + DCTX_BUF]
	 */
	instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_FP, DT_STK_DCTX);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_LOAD(BPF_DW, BPF_REG_9, BPF_REG_0, DCTX_BUF);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

	/*
	 *	dctx->mst->fault = 0;	// lddw %r0, [%r0 + DCTX_MST]
	 *				// stdw [%r0 + DMST_FAULT], 0
	 *	dctx->mst->tstamp = 0;	// stdw [%r0 + DMST_TSTAMP], 0
	 *	dctx->mst->epid = EPID;	// stw [%r0 + DMST_EPID], EPID
	 *	dctx->mst->prid = PRID;	// stw [%r0 + DMST_PRID], PRID
	 *	*((uint32_t *)&buf[0]) = EPID;
	 *				// stw [%r9 + 0], EPID
	 */
	instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_0, DCTX_MST);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE_IMM(BPF_DW, BPF_REG_0, DMST_FAULT, 0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE_IMM(BPF_DW, BPF_REG_0, DMST_TSTAMP, 0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE_IMM(BPF_W, BPF_REG_0, DMST_EPID, -1);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	dlp->dl_last->di_extern = epid;
	instr = BPF_STORE_IMM(BPF_W, BPF_REG_0, DMST_PRID, -1);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	dlp->dl_last->di_extern = prid;
	instr = BPF_STORE_IMM(BPF_W, BPF_REG_9, 0, -1);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	dlp->dl_last->di_extern = epid;

	/*
	 *	dctx->mst->tag = 0;	// stw [%r0 + DMST_TAG], 0
	 *	*((uint32_t *)&buf[4]) = 0;
	 *				// stw [%r9 + 4], 0
	 */
	instr = BPF_STORE_IMM(BPF_W, BPF_REG_0, DMST_TAG, 0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_STORE_IMM(BPF_W, BPF_REG_9, 4, 0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

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
		instr = BPF_BRANCH_IMM(BPF_JEQ, pred->dn_reg, 0,
				       pcb->pcb_exitlbl);
		TRACE_REGSET("    Pred: Value");
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
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
 *	4. If a fault was flagged, return 0.
 *	5. Submit the buffer to the perf event output buffer for the current
 *	   cpu.
 *	6. Return 0
 * }
 */
static void
dt_cg_epilogue(dt_pcb_t *pcb)
{
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	dt_ident_t	*buffers = dt_dlib_get_map(pcb->pcb_hdl, "buffers");
	struct bpf_insn	instr;

	assert(buffers != NULL);

	/*
	 *	rc = dctx->mst->fault;	// lddw %r0, [%fp + DT_STK_DCTX]
	 *				// lddw %r0, [%r0 + DCTX_MST]
	 *				// lddw %r0, [%r0 + DMST_FAULT]
	 *	if (rc != 0)
	 *	    goto exit;		// jne %r0, 0, pcb->pcb_exitlbl
	 */
	TRACE_REGSET("Epilogue: Begin");
	instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_FP, DT_STK_DCTX);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_0, DCTX_MST);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_0, DMST_FAULT);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_BRANCH_IMM(BPF_JNE, BPF_REG_0, 0, pcb->pcb_exitlbl);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

	/*
	 *	bpf_perf_event_output(dctx->ctx, &buffers, BPF_F_CURRENT_CPU,
	 *			      buf - 4, bufoff + 4);
	 *				// lddw %r1, [%fp + DT_STK_DCTX]
	 *				// lddw %r1, [%r1 + DCTX_CTX]
	 *				// lddw %r2, &buffers
	 *				// lddw %r3, BPF_F_CURRENT_CPU
	 *				// mov %r4, %r9
	 *				// add %r4, -4
	 *				// mov %r5, pcb->pcb_bufoff
	 *				// add %r4, 4
	 *				// call bpf_perf_event_output
	 *
	 * exit:
	 *	return 0;		// mov %r0, 0
	 *				// exit
	 * }
	 */
	instr = BPF_LOAD(BPF_DW, BPF_REG_1, BPF_REG_FP, DT_STK_DCTX);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_LOAD(BPF_DW, BPF_REG_1, BPF_REG_1, DCTX_CTX);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	dt_cg_xsetx(dlp, buffers, DT_LBL_NONE, BPF_REG_2, buffers->di_id);
	dt_cg_xsetx(dlp, NULL, DT_LBL_NONE, BPF_REG_3, BPF_F_CURRENT_CPU);
	instr = BPF_MOV_REG(BPF_REG_4, BPF_REG_9);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_ALU64_IMM(BPF_ADD, BPF_REG_4, -4);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_MOV_IMM(BPF_REG_5, pcb->pcb_bufoff);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_ALU64_IMM(BPF_ADD, BPF_REG_5, 4);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_CALL_HELPER(BPF_FUNC_perf_event_output);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_MOV_IMM(BPF_REG_0, 0);
	dt_irlist_append(dlp, dt_cg_node_alloc(pcb->pcb_exitlbl, instr));
	instr = BPF_RETURN();
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	TRACE_REGSET("Epilogue: End  ");
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
	struct bpf_insn	instr;
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	uint_t		off = pcb->pcb_bufoff;

	if (gap & 1) {
		instr = BPF_STORE_IMM(BPF_B, BPF_REG_9, off, 0);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		off += 1;
	}
	if (gap & 2) {
		instr = BPF_STORE_IMM(BPF_H, BPF_REG_9, off, 0);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		off += 2;
	}
	if (gap & 4) {
		instr = BPF_STORE_IMM(BPF_W, BPF_REG_9, off, 0);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	}
}

static void
dt_cg_spill_store(int reg)
{
	dt_irlist_t	*dlp = &yypcb->pcb_ir;
	struct bpf_insn	instr;

	instr = BPF_STORE(BPF_DW, BPF_REG_FP, DT_STK_SPILL(reg), reg);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
}

static void
dt_cg_spill_load(int reg)
{
	dt_irlist_t	*dlp = &yypcb->pcb_ir;
	struct bpf_insn	instr;

	instr = BPF_LOAD(BPF_DW, reg, BPF_REG_FP, DT_STK_SPILL(reg));
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
}

static int
dt_cg_store_val(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind,
		dt_pfargv_t *pfp, int arg)
{
	dtrace_diftype_t	vtype;
	struct bpf_insn		instr;
	dt_irlist_t		*dlp = &pcb->pcb_ir;
	uint_t			off;

	dt_cg_node(dnp, &pcb->pcb_ir, pcb->pcb_regs);
	dt_node_diftype(pcb->pcb_hdl, dnp, &vtype);

	if (dt_node_is_scalar(dnp) || dt_node_is_float(dnp)) {
		int	sz = 8;

		off = dt_rec_add(pcb->pcb_hdl, dt_cg_fill_gap, kind,
				 vtype.dtdt_size, vtype.dtdt_size, pfp,
				 arg);

		switch (vtype.dtdt_size) {
		case sizeof(uint64_t):
			sz = BPF_DW;
			break;
		case sizeof(uint32_t):
			sz = BPF_W;
			break;
		case sizeof(uint16_t):
			sz = BPF_H;
			break;
		case sizeof(uint8_t):
			sz = BPF_B;
			break;
		}

		instr = BPF_STORE(sz, BPF_REG_9, off, dnp->dn_reg);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		dt_regset_free(pcb->pcb_regs, dnp->dn_reg);

		return 0;
#if 0
	} else if (dt_node_is_string(dnp->dn_args)) {
		size_t sz = dt_node_type_size(dnp->dn_args);

		/*
		 * Strings are stored as a 64-bit size followed by a character
		 * array.  Given that all entries in the output buffer are
		 * aligned at 64-bit boundaries, this guarantees that the
		 * character array is also aligned at a 64-bit boundary.
		 * We will pad the string to a multiple of 8 bytes as well.
		 *
		 * We store the size as two 32-bit values, lower 4 bytes first,
		 * then the higher 4 bytes.
		 */
		sz = P2ROUNDUP(sz, sizeof(uint64_t));
		instr = BPF_STORE_IMM(BPF_W, BPF_REG_9, off,
				      sz & ((1UL << 32)-1));
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		instr = BPF_STORE_IMM(BPF_W, BPF_REG_9, off + 4, sz >> 32);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		dt_regset_free(pcb->pcb_regs, dnp->dn_args->dn_reg);

		return sz + sizeof(uint64_t);
#endif
	}

	return -1;
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
	dt_node_t *anp;
	dt_ident_t *aid;
	char n[DT_TYPE_NAMELEN];

	anp = dnp->dn_args;
	assert(anp != NULL);
	if (anp->dn_kind != DT_NODE_AGG) {
		dnerror(dnp, D_CLEAR_AGGARG,
			"%s( ) argument #1 is incompatible with prototype:\n"
			"\tprototype: aggregation\n\t argument: %s\n",
			dnp->dn_ident->di_name,
			dt_node_type_name(anp, n, sizeof(n)));
	}

	aid = anp->dn_ident;
        if (aid->di_gen == pcb->pcb_hdl->dt_gen &&
	    !(aid->di_flags & DT_IDFLG_MOD)) {
		dnerror(dnp, D_CLEAR_AGGBAD,
			"undefined aggregation: @%s\n", aid->di_name);
	}

	/*
	 * FIXME: Needs implementation
	 * TODO: Emit code to clear the given aggregation.
	 * DEPENDS ON: How aggregations are implemented using eBPF (hashmap?).
	 * AGGID = aid->di_id
	 */
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
	struct bpf_insn instr;
	dt_irlist_t *dlp = &pcb->pcb_ir;
	uint_t off;

	dt_cg_node(dnp->dn_args, &pcb->pcb_ir, pcb->pcb_regs);

	off = dt_rec_add(pcb->pcb_hdl, dt_cg_fill_gap, DTRACEACT_COMMIT,
			 sizeof(uint64_t), sizeof(uint64_t), NULL,
			 DT_ACT_COMMIT);

	instr = BPF_STORE(BPF_DW, BPF_REG_9, off, BPF_REG_0);	/* FIXME */
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
}

static void
dt_cg_act_denormalize(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind)
{
	dt_node_t *anp;
	dt_ident_t *aid;
	char n[DT_TYPE_NAMELEN];

	anp = dnp->dn_args;
	assert(anp != NULL);
	if (anp->dn_kind != DT_NODE_AGG) {
		dnerror(dnp, D_NORMALIZE_AGGARG,
			"%s( ) argument #1 is incompatible with prototype:\n"
			"\tprototype: aggregation\n\t argument: %s\n",
			dnp->dn_ident->di_name,
			dt_node_type_name(anp, n, sizeof(n)));
	}

	aid = anp->dn_ident;
        if (aid->di_gen == pcb->pcb_hdl->dt_gen &&
	    !(aid->di_flags & DT_IDFLG_MOD)) {
		dnerror(dnp, D_NORMALIZE_AGGBAD,
			"undefined aggregation: @%s\n", aid->di_name);
	}

	/*
	 * FIXME: Needs implementation
	 * TODO: Emit code to clear the given aggregation.
	 * DEPENDS ON: How aggregations are implemented using eBPF (hashmap?).
	 * AGGID = aid->di_id
	 */
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
	struct bpf_insn	instr;
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	uint_t		off;

	dt_cg_node(dnp->dn_args, &pcb->pcb_ir, pcb->pcb_regs);

	off = dt_rec_add(pcb->pcb_hdl, dt_cg_fill_gap, DTRACEACT_DISCARD,
			 sizeof(uint64_t), sizeof(uint64_t), NULL,
			 DT_ACT_DISCARD);

	instr = BPF_STORE(BPF_DW, BPF_REG_9, off, BPF_REG_0);	/* FIXME */
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
}

/*
 * Signal that tracing should be terminated with the given return code.
 *
 * Stores:
 *	integer (4 bytes)		-- return code
 */
static void
dt_cg_act_exit(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind)
{
	struct bpf_insn	instr;
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	uint_t		off;

	dt_cg_node(dnp->dn_args, &pcb->pcb_ir, pcb->pcb_regs);

	off = dt_rec_add(pcb->pcb_hdl, dt_cg_fill_gap, DTRACEACT_EXIT,
			 sizeof(uint32_t), sizeof(uint32_t), NULL,
			 DT_ACT_EXIT);

	instr = BPF_STORE(BPF_W, BPF_REG_9, off, dnp->dn_args->dn_reg);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	dt_regset_free(pcb->pcb_regs, dnp->dn_args->dn_reg);
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
	struct bpf_insn	instr;
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	dt_regset_t	*drp = pcb->pcb_regs;

	TRACE_REGSET("raise(): Begin ");

	dt_cg_node(dnp->dn_args, &pcb->pcb_ir, drp);

	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	instr = BPF_MOV_REG(BPF_REG_1, dnp->dn_args->dn_reg);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	dt_regset_free(drp, dnp->dn_args->dn_reg);
	dt_regset_xalloc(drp, BPF_REG_0);
	instr = BPF_CALL_HELPER(BPF_FUNC_send_signal);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
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
	struct bpf_insn	instr;
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	uint_t		off;

	dt_cg_node(dnp->dn_args, &pcb->pcb_ir, pcb->pcb_regs);

	off = dt_rec_add(pcb->pcb_hdl, dt_cg_fill_gap, DTRACEACT_SPECULATE,
			 sizeof(uint64_t), sizeof(uint64_t), NULL,
			 DT_ACT_SPECULATE);

	instr = BPF_STORE(BPF_DW, BPF_REG_9, off, BPF_REG_0);	/* FIXME */
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
}

static void
dt_cg_act_stack(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind)
{
	dt_node_t *arg = dnp->dn_args;
	uint32_t nframes = 0;

	if (arg != NULL) {
		if (!dt_node_is_posconst(arg)) {
			dnerror(arg, D_STACK_SIZE, "stack( ) argument #1 must "
				"be a non-zero positive integer constant\n");
		}

		nframes = (uint32_t)arg->dn_value;
	}
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
}

static void
dt_cg_act_trace(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind)
{
	char		n[DT_TYPE_NAMELEN];
	dt_node_t	*arg = dnp->dn_args;
	int		type = 0;

	if (dt_node_is_void(arg)) {
		dnerror(arg, D_TRACE_VOID,
			"trace( ) may not be applied to a void expression\n");
	}

	if (dt_node_is_dynamic(arg)) {
		dnerror(arg, D_TRACE_DYN,
			"trace( ) may not be applied to a dynamic "
			"expression\n");
	}

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
}

static void
dt_cg_act_ustack(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind)
{
	dt_node_t *arg0 = dnp->dn_args;
	dt_node_t *arg1 = arg0 != NULL ? arg0->dn_list : NULL;
	uint32_t nframes = 0;
	uint32_t strsize = 0;

	if (arg0 != NULL) {
		if (!dt_node_is_posconst(arg0)) {
			dnerror(arg0, D_USTACK_FRAMES, "ustack( ) argument #1 "
				"must be a non-zero positive integer "
				"constant\n");
		}

		nframes = (uint32_t)arg0->dn_value;
	}

	if (arg1 != NULL) {
		if (arg1->dn_kind != DT_NODE_INT ||
		    ((arg1->dn_flags & DT_NF_SIGNED) &&
		    (int64_t)arg1->dn_value < 0)) {
			dnerror(arg1, D_USTACK_STRSIZE, "ustack( ) argument #2 "
				"must be a positive integer constant\n");
		}

		strsize = (uint32_t)arg1->dn_value;
	}
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
	[DT_ACT_IDX(DT_ACT_STOP)]		= { &dt_cg_act_stop, },
	[DT_ACT_IDX(DT_ACT_BREAKPOINT)]		= { &dt_cg_act_breakpoint, },
	[DT_ACT_IDX(DT_ACT_PANIC)]		= { &dt_cg_act_panic, },
	[DT_ACT_IDX(DT_ACT_SPECULATE)]		= { &dt_cg_act_speculate, },
	[DT_ACT_IDX(DT_ACT_COMMIT)]		= { &dt_cg_act_commit, },
	[DT_ACT_IDX(DT_ACT_DISCARD)]		= { &dt_cg_act_discard, },
	[DT_ACT_IDX(DT_ACT_CHILL)]		= { &dt_cg_act_chill, },
	[DT_ACT_IDX(DT_ACT_EXIT)]		= { &dt_cg_act_exit, },
	[DT_ACT_IDX(DT_ACT_USTACK)]		= { &dt_cg_act_ustack, },
	[DT_ACT_IDX(DT_ACT_PRINTA)]		= { &dt_cg_act_printa, },
	[DT_ACT_IDX(DT_ACT_RAISE)]		= { &dt_cg_act_raise, },
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
	dt_irnode_t *dip = malloc(sizeof (dt_irnode_t));

	if (dip == NULL)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOMEM);

	dip->di_label = label;
	dip->di_instr = instr;
	dip->di_extern = NULL;
	dip->di_next = NULL;

	return (dip);
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

		if (ctf_type_name(fp, type, n, sizeof (n)) == NULL ||
		    dt_type_lookup(n, &dtt) == -1 || (
		    dtt.dtt_ctfp == fp && dtt.dtt_type == type))
			break; /* unable to improve our position */

		fp = dtt.dtt_ctfp;
		type = ctf_type_resolve(fp, dtt.dtt_type);
	}

	if (ctf_member_info(fp, type, s, mp) == CTF_ERR)
		return (NULL); /* ctf_errno is set for us */

	return (fp);
}

static void
dt_cg_xsetx(dt_irlist_t *dlp, dt_ident_t *idp, uint_t lbl, int reg, uint64_t x)
{
	struct bpf_insn instr[2] = { BPF_LDDW(reg, x) };

	dt_irlist_append(dlp, dt_cg_node_alloc(lbl, instr[0]));
	if (idp != NULL)
		dlp->dl_last->di_extern = idp;
	dt_irlist_append(dlp, dt_cg_node_alloc(lbl, instr[1]));
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

	return (x + 1);
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
	static const uint_t ops[] = {
		BPF_B,	BPF_H,	0,	BPF_W,
		0,	0,	0,	BPF_DW,
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

	return ops[size];
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

	return (ops[size]);
#endif
}

static void
dt_cg_load_var(dt_node_t *dst, dt_irlist_t *dlp, dt_regset_t *drp)
{
	struct bpf_insn	instr;
	dt_ident_t	*idp = dt_ident_resolve(dst->dn_ident);

	idp->di_flags |= DT_IDFLG_DIFR;
	if (idp->di_flags & DT_IDFLG_LOCAL) {		/* local var */
		if ((dst->dn_reg = dt_regset_alloc(drp)) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

		/*
		 * If this is the first read for this local variable, we know
		 * the value is 0.  This avoids storing an initial 0 value in
		 * the variable's stack location.
		 */
		if (!(idp->di_flags & DT_IDFLG_DIFW))
			instr = BPF_MOV_IMM(dst->dn_reg, 0);
		else
			instr = BPF_LOAD(BPF_DW, dst->dn_reg, BPF_REG_FP,
					 DT_STK_LVAR(idp->di_id));

		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	} else if (idp->di_flags & DT_IDFLG_TLS) {	/* TLS var */
		if (dt_regset_xalloc_args(drp) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
		instr = BPF_MOV_IMM(BPF_REG_1,
				    idp->di_id - DIF_VAR_OTHER_UBASE);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		idp = dt_dlib_get_func(yypcb->pcb_hdl, "dt_get_tvar");
		assert(idp != NULL);
		dt_regset_xalloc(drp, BPF_REG_0);
		instr = BPF_CALL_FUNC(idp->di_id);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		dlp->dl_last->di_extern = idp;
		dt_regset_free_args(drp);

		if ((dst->dn_reg = dt_regset_alloc(drp)) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

		instr = BPF_MOV_REG(dst->dn_reg, BPF_REG_0);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		dt_regset_free(drp, BPF_REG_0);
	} else {					/* global var */
		if (dt_regset_xalloc_args(drp) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
		if (idp->di_id < DIF_VAR_OTHER_UBASE) {	/* built-in var */
			instr = BPF_LOAD(BPF_DW, BPF_REG_1,
					 BPF_REG_FP, DT_STK_DCTX);
			dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE,
					 instr));
			instr = BPF_LOAD(BPF_DW, BPF_REG_1,
					 BPF_REG_1, DCTX_MST);
			dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE,
					 instr));
			instr = BPF_MOV_IMM(BPF_REG_2, idp->di_id);
			dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE,
					 instr));
			idp = dt_dlib_get_func(yypcb->pcb_hdl, "dt_get_bvar");
		} else {
			instr = BPF_MOV_IMM(BPF_REG_1,
					    idp->di_id - DIF_VAR_OTHER_UBASE);
			dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE,
					 instr));
			idp = dt_dlib_get_func(yypcb->pcb_hdl, "dt_get_gvar");
		}
		assert(idp != NULL);
		dt_regset_xalloc(drp, BPF_REG_0);
		instr = BPF_CALL_FUNC(idp->di_id);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		dlp->dl_last->di_extern = idp;
		dt_regset_free_args(drp);

		if ((dst->dn_reg = dt_regset_alloc(drp)) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

		instr = BPF_MOV_REG(dst->dn_reg, BPF_REG_0);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		dt_regset_free(drp, BPF_REG_0);
	}
}

static void
dt_cg_ptrsize(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp,
    uint_t op, int dreg)
{
	ctf_file_t *ctfp = dnp->dn_ctfp;
	ctf_arinfo_t r;
	struct bpf_insn instr;
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

	instr = BPF_ALU64_IMM(op, dreg, size);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
}

/*
 * If the result of a "." or "->" operation is a bit-field, we use this routine
 * to generate an epilogue to the load instruction that extracts the value.  In
 * the diagrams below the "ld??" is the load instruction that is generated to
 * load the containing word that is generating prior to calling this function.
 *
 * Epilogue for unsigned fields:	Epilogue for signed fields:
 *
 * ldu?	[r1], r1			lds? [r1], r1
 * setx	USHIFT, r2			setx 64 - SSHIFT, r2
 * srl	r1, r2, r1			sll  r1, r2, r1
 * setx	(1 << bits) - 1, r2		setx 64 - bits, r2
 * and	r1, r2, r1			sra  r1, r2, r1
 *
 * The *SHIFT constants above changes value depending on the endian-ness of our
 * target architecture.  Refer to the comments below for more details.
 */
static void
dt_cg_field_get(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp,
    ctf_file_t *fp, const ctf_membinfo_t *mp)
{
	ctf_encoding_t e;
	struct bpf_insn instr;
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
	 * nearest power of two to this value (see clp2(), above).  These
	 * properties are used to compute shift as USHIFT or SSHIFT, below.
	 */
	if (dnp->dn_flags & DT_NF_SIGNED) {
#ifdef _BIG_ENDIAN
		shift = clp2(P2ROUNDUP(e.cte_bits, NBBY) / NBBY) * NBBY -
		    mp->ctm_offset % NBBY;
#else
		shift = mp->ctm_offset % NBBY + e.cte_bits;
#endif
		instr = BPF_ALU64_IMM(BPF_LSH, r1, 64 - shift);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

		instr = BPF_ALU64_IMM(BPF_ARSH, r1, 64 - e.cte_bits);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	} else {
#ifdef _BIG_ENDIAN
		shift = clp2(P2ROUNDUP(e.cte_bits, NBBY) / NBBY) * NBBY -
		    (mp->ctm_offset % NBBY + e.cte_bits);
#else
		shift = mp->ctm_offset % NBBY;
#endif
		instr = BPF_ALU64_IMM(BPF_LSH, r1, shift);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

		instr = BPF_ALU64_IMM(BPF_AND, r1, (1ULL << e.cte_bits) - 1);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	}
}

/*
 * If the destination of a store operation is a bit-field, we use this routine
 * to generate a prologue to the store instruction that loads the surrounding
 * bits, clears the destination field, and ORs in the new value of the field.
 * In the diagram below the "st?" is the store instruction that is generated to
 * store the containing word that is generating after calling this function.
 *
 * ld	[dst->dn_reg], r1
 * setx	~(((1 << cte_bits) - 1) << (ctm_offset % NBBY)), r2
 * and	r1, r2, r1
 *
 * setx	(1 << cte_bits) - 1, r2
 * and	src->dn_reg, r2, r2
 * setx ctm_offset % NBBY, r3
 * sll	r2, r3, r2
 *
 * or	r1, r2, r1
 * st?	r1, [dst->dn_reg]
 *
 * This routine allocates a new register to hold the value to be stored and
 * returns it.  The caller is responsible for freeing this register later.
 */
static int
dt_cg_field_set(dt_node_t *src, dt_irlist_t *dlp,
    dt_regset_t *drp, dt_node_t *dst)
{
	uint64_t cmask, fmask, shift;
	struct bpf_insn instr;
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

	/* FIXME: Does not handled signed or userland */
	instr = BPF_LOAD(dt_cg_load(dst, fp, m.ctm_type), r1, dst->dn_reg, 0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

	instr = BPF_ALU64_IMM(BPF_AND, r1, cmask);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

	dt_cg_setx(dlp, r2, fmask);
	instr = BPF_ALU64_REG(BPF_AND, r1, r2);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

	instr = BPF_ALU64_IMM(BPF_LSH, r2, shift);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

	instr = BPF_ALU64_REG(BPF_OR, r1, r2);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

	dt_regset_free(drp, r2);

	return (r1);
}

static void
dt_cg_store(dt_node_t *src, dt_irlist_t *dlp, dt_regset_t *drp, dt_node_t *dst)
{
	ctf_encoding_t e;
	struct bpf_insn instr;
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
		size = dt_node_type_size(src);

	if (src->dn_flags & DT_NF_REF) {
#ifdef FIXME
		if ((reg = dt_regset_alloc(drp)) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
		dt_cg_setx(dlp, reg, size);
		instr = DIF_INSTR_COPYS(src->dn_reg, reg, dst->dn_reg);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		dt_regset_free(drp, reg);
#else
		xyerror(D_UNKNOWN, "internal error -- cg cannot store "
			"values passed by ref\n");
#endif
	} else {
		if (dst->dn_flags & DT_NF_BITFIELD)
			reg = dt_cg_field_set(src, dlp, drp, dst);
		else
			reg = src->dn_reg;

		switch (size) {
		case 1:
			instr = BPF_STORE(BPF_B, dst->dn_reg, 0, reg);
			break;
		case 2:
			instr = BPF_STORE(BPF_H, dst->dn_reg, 0, reg);
			break;
		case 4:
			instr = BPF_STORE(BPF_W, dst->dn_reg, 0, reg);
			break;
		case 8:
			instr = BPF_STORE(BPF_DW, dst->dn_reg, 0, reg);
			break;
		default:
			xyerror(D_UNKNOWN, "internal error -- cg cannot store "
			    "size %lu when passed by value\n", (ulong_t)size);
		}
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

		if (dst->dn_flags & DT_NF_BITFIELD)
			dt_regset_free(drp, reg);
	}
}

static void
dt_cg_store_var(dt_node_t *src, dt_irlist_t *dlp, dt_regset_t *drp,
		dt_ident_t *idp)
{
	struct bpf_insn	instr;

	idp->di_flags |= DT_IDFLG_DIFW;
	if (idp->di_flags & DT_IDFLG_LOCAL) {		/* local var */
		instr = BPF_STORE(BPF_DW, BPF_REG_FP, DT_STK_LVAR(idp->di_id),
				  src->dn_reg);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	} else if (idp->di_flags & DT_IDFLG_TLS) {	/* TLS var */
		if (dt_regset_xalloc_args(drp) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
		instr = BPF_MOV_REG(BPF_REG_2, src->dn_reg);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		instr = BPF_MOV_IMM(BPF_REG_1,
				    idp->di_id - DIF_VAR_OTHER_UBASE);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		idp = dt_dlib_get_func(yypcb->pcb_hdl, "dt_set_tvar");
		assert(idp != NULL);
		dt_regset_xalloc(drp, BPF_REG_0);
		instr = BPF_CALL_FUNC(idp->di_id);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		dlp->dl_last->di_extern = idp;
		dt_regset_free(drp, BPF_REG_0);
		dt_regset_free_args(drp);
	} else {					/* global var */
		if (dt_regset_xalloc_args(drp) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
		instr = BPF_MOV_REG(BPF_REG_2, src->dn_reg);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		instr = BPF_MOV_IMM(BPF_REG_1,
				    idp->di_id - DIF_VAR_OTHER_UBASE);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		idp = dt_dlib_get_func(yypcb->pcb_hdl, "dt_set_gvar");
		assert(idp != NULL);
		dt_regset_xalloc(drp, BPF_REG_0);
		instr = BPF_CALL_FUNC(idp->di_id);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		dlp->dl_last->di_extern = idp;
		dt_regset_free(drp, BPF_REG_0);
		dt_regset_free_args(drp);
	}
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
	struct bpf_insn instr;
	int n;

	/* If the destination type is '@' (any type) we need not cast. */
	if (dst->dn_ctfp == NULL && dst->dn_type == CTF_ERR)
		return;

	srcsize = dt_node_type_size(src);
	dstsize = dt_node_type_size(dst);

	if (dstsize < srcsize)
		n = sizeof (uint64_t) * NBBY - dstsize * NBBY;
	else
		n = sizeof (uint64_t) * NBBY - srcsize * NBBY;

	if (dt_node_is_scalar(dst) && n != 0 && (dstsize < srcsize ||
	    (src->dn_flags & DT_NF_SIGNED) ^ (dst->dn_flags & DT_NF_SIGNED))) {
		instr = BPF_MOV_REG(dst->dn_reg, src->dn_reg);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		instr = BPF_ALU64_IMM(BPF_LSH, dst->dn_reg, n);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		instr = BPF_ALU64_IMM((dst->dn_flags & DT_NF_SIGNED) ?
		    BPF_ARSH : BPF_RSH, dst->dn_reg, n);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
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
		struct bpf_insn instr;
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
#else
		instr = BPF_CALL_FUNC(op);
#endif
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
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

	struct bpf_insn instr;

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

		instr = BPF_BRANCH_IMM(BPF_JSLT, dnp->dn_left->dn_reg, 0, lbl_L3);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		instr = BPF_BRANCH_IMM(BPF_JSLT, dnp->dn_right->dn_reg, 0, lbl_L1);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		instr = BPF_JUMP(lbl_L4);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		instr = BPF_NEG_REG(dnp->dn_right->dn_reg);
		dt_irlist_append(dlp, dt_cg_node_alloc(lbl_L1, instr));
		instr = BPF_ALU64_REG(op, dnp->dn_left->dn_reg,
		    dnp->dn_right->dn_reg);
		dt_irlist_append(dlp, dt_cg_node_alloc(lbl_L2, instr));
		instr = BPF_NEG_REG(dnp->dn_left->dn_reg);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		instr = BPF_JUMP(lbl_L5);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		instr = BPF_NEG_REG(dnp->dn_left->dn_reg);
		dt_irlist_append(dlp, dt_cg_node_alloc(lbl_L3, instr));
		instr = BPF_BRANCH_IMM(BPF_JSGE, dnp->dn_right->dn_reg, 0, lbl_L2);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		instr = BPF_NEG_REG(dnp->dn_right->dn_reg);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		instr = BPF_ALU64_REG(op, dnp->dn_left->dn_reg,
		    dnp->dn_right->dn_reg);
		dt_irlist_append(dlp, dt_cg_node_alloc(lbl_L4, instr));
		instr = BPF_NOP();
		dt_irlist_append(dlp, dt_cg_node_alloc(lbl_L5, instr));
	} else {
		instr = BPF_ALU64_REG(op, dnp->dn_left->dn_reg,
		    dnp->dn_right->dn_reg);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	}

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

	return (idp->di_kind == DT_IDENT_ARRAY ? aops[i] : sops[i]);
}
#endif

static void
dt_cg_prearith_op(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp, uint_t op)
{
	ctf_file_t *ctfp = dnp->dn_ctfp;
	struct bpf_insn instr;
	ctf_id_t type;
	ssize_t size = 1;

	if (dt_node_is_pointer(dnp)) {
		type = ctf_type_resolve(ctfp, dnp->dn_type);
		assert(ctf_type_kind(ctfp, type) == CTF_K_POINTER);
		size = ctf_type_size(ctfp, ctf_type_reference(ctfp, type));
	}

	dt_cg_node(dnp->dn_child, dlp, drp);
	dnp->dn_reg = dnp->dn_child->dn_reg;

	instr = BPF_ALU64_IMM(op, dnp->dn_reg, size);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

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
	struct bpf_insn instr;
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

	instr = BPF_MOV_REG(nreg, oreg);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	instr = BPF_ALU64_IMM(op, nreg, size);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

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
		return (1); /* strings always compare signed */
	else if (!dt_node_is_arith(dnp->dn_left) ||
	    !dt_node_is_arith(dnp->dn_right))
		return (0); /* non-arithmetic types always compare unsigned */

	memset(&dn, 0, sizeof (dn));
	dt_node_promote(dnp->dn_left, dnp->dn_right, &dn);
	return (dn.dn_flags & DT_NF_SIGNED);
}

static void
dt_cg_compare_op(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp, uint_t op)
{
	uint_t lbl_true = dt_irlist_label(dlp);
	uint_t lbl_post = dt_irlist_label(dlp);

	struct bpf_insn instr;

	dt_cg_node(dnp->dn_left, dlp, drp);
	dt_cg_node(dnp->dn_right, dlp, drp);

	/* FIXME: No support for string comparison yet */
	if (dt_node_is_string(dnp->dn_left) || dt_node_is_string(dnp->dn_right))
		xyerror(D_UNKNOWN, "internal error -- no support for "
			"string comparison yet\n");

	instr = BPF_BRANCH_REG(op, dnp->dn_left->dn_reg, dnp->dn_right->dn_reg,
			       lbl_true);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	dt_regset_free(drp, dnp->dn_right->dn_reg);
	dnp->dn_reg = dnp->dn_left->dn_reg;

	instr = BPF_MOV_IMM(dnp->dn_reg, 0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

	instr = BPF_JUMP(lbl_post);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

	instr = BPF_MOV_IMM(dnp->dn_reg, 1);
	dt_irlist_append(dlp, dt_cg_node_alloc(lbl_true, instr));
	dt_irlist_append(dlp, dt_cg_node_alloc(lbl_post, BPF_NOP()));
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

	struct bpf_insn instr;
	dt_irnode_t *dip;

	dt_cg_node(dnp->dn_expr, dlp, drp);
	instr = BPF_BRANCH_IMM(BPF_JEQ, dnp->dn_expr->dn_reg, 0, lbl_false);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	dt_regset_free(drp, dnp->dn_expr->dn_reg);

	dt_cg_node(dnp->dn_left, dlp, drp);
	instr = BPF_MOV_IMM(dnp->dn_left->dn_reg, 0);
	dip = dt_cg_node_alloc(DT_LBL_NONE, instr); /* save dip for below */
	dt_irlist_append(dlp, dip);
	dt_regset_free(drp, dnp->dn_left->dn_reg);

	instr = BPF_JUMP(lbl_post);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

	dt_irlist_append(dlp, dt_cg_node_alloc(lbl_false, BPF_NOP()));
	dt_cg_node(dnp->dn_right, dlp, drp);
	dnp->dn_reg = dnp->dn_right->dn_reg;

	/*
	 * Now that dn_reg is assigned, reach back and patch the correct MOV
	 * instruction into the tail of dn_left.  We know dn_reg was unused
	 * at that point because otherwise dn_right couldn't have allocated it.
	 */
	dip->di_instr = BPF_MOV_REG(dnp->dn_reg, dnp->dn_left->dn_reg);
	dt_irlist_append(dlp, dt_cg_node_alloc(lbl_post, BPF_NOP()));
}

static void
dt_cg_logical_and(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	uint_t lbl_false = dt_irlist_label(dlp);
	uint_t lbl_post = dt_irlist_label(dlp);

	struct bpf_insn instr;

	dt_cg_node(dnp->dn_left, dlp, drp);
	instr = BPF_BRANCH_IMM(BPF_JEQ, dnp->dn_left->dn_reg, 0, lbl_false);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	dt_regset_free(drp, dnp->dn_left->dn_reg);

	dt_cg_node(dnp->dn_right, dlp, drp);
	instr = BPF_BRANCH_IMM(BPF_JEQ, dnp->dn_right->dn_reg, 0, lbl_false);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	dnp->dn_reg = dnp->dn_right->dn_reg;

	dt_cg_setx(dlp, dnp->dn_reg, 1);

	instr = BPF_JUMP(lbl_post);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

	instr = BPF_MOV_IMM(dnp->dn_reg, 0);
	dt_irlist_append(dlp, dt_cg_node_alloc(lbl_false, instr));

	dt_irlist_append(dlp, dt_cg_node_alloc(lbl_post, BPF_NOP()));
}

static void
dt_cg_logical_xor(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	uint_t lbl_next = dt_irlist_label(dlp);
	uint_t lbl_tail = dt_irlist_label(dlp);

	struct bpf_insn instr;

	dt_cg_node(dnp->dn_left, dlp, drp);
	instr = BPF_BRANCH_IMM(BPF_JEQ, dnp->dn_left->dn_reg, 0, lbl_next);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	dt_cg_setx(dlp, dnp->dn_left->dn_reg, 1);

	dt_irlist_append(dlp, dt_cg_node_alloc(lbl_next, BPF_NOP()));

	dt_cg_node(dnp->dn_right, dlp, drp);
	instr = BPF_BRANCH_IMM(BPF_JEQ, dnp->dn_right->dn_reg, 0, lbl_tail);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	dt_cg_setx(dlp, dnp->dn_right->dn_reg, 1);

	instr = BPF_ALU64_REG(BPF_XOR, dnp->dn_left->dn_reg,
			      dnp->dn_right->dn_reg);

	dt_irlist_append(dlp, dt_cg_node_alloc(lbl_tail, instr));

	dt_regset_free(drp, dnp->dn_right->dn_reg);
	dnp->dn_reg = dnp->dn_left->dn_reg;
}

static void
dt_cg_logical_or(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	uint_t lbl_true = dt_irlist_label(dlp);
	uint_t lbl_false = dt_irlist_label(dlp);
	uint_t lbl_post = dt_irlist_label(dlp);

	struct bpf_insn instr;

	dt_cg_node(dnp->dn_left, dlp, drp);
	instr = BPF_BRANCH_IMM(BPF_JNE, dnp->dn_left->dn_reg, 0, lbl_true);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	dt_regset_free(drp, dnp->dn_left->dn_reg);

	dt_cg_node(dnp->dn_right, dlp, drp);
	instr = BPF_BRANCH_IMM(BPF_JEQ, dnp->dn_right->dn_reg, 0, lbl_false);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
	dnp->dn_reg = dnp->dn_right->dn_reg;

	instr = BPF_MOV_IMM(dnp->dn_reg, 1);
	dt_irlist_append(dlp, dt_cg_node_alloc(lbl_true, instr));

	instr = BPF_JUMP(lbl_post);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

	instr = BPF_MOV_IMM(dnp->dn_reg, 0);
	dt_irlist_append(dlp, dt_cg_node_alloc(lbl_false, instr));

	dt_irlist_append(dlp, dt_cg_node_alloc(lbl_post, BPF_NOP()));
}

static void
dt_cg_logical_neg(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	uint_t lbl_zero = dt_irlist_label(dlp);
	uint_t lbl_post = dt_irlist_label(dlp);

	struct bpf_insn instr;

	dt_cg_node(dnp->dn_child, dlp, drp);
	dnp->dn_reg = dnp->dn_child->dn_reg;

	instr = BPF_BRANCH_IMM(BPF_JEQ, dnp->dn_reg, 0, lbl_zero);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

	instr = BPF_MOV_IMM(dnp->dn_reg, 0);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

	instr = BPF_JUMP(lbl_post);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

	instr = BPF_MOV_IMM(dnp->dn_reg, 1);
	dt_irlist_append(dlp, dt_cg_node_alloc(lbl_zero, instr));
	dt_irlist_append(dlp, dt_cg_node_alloc(lbl_post, BPF_NOP()));
}

static void
dt_cg_asgn_op(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	struct bpf_insn instr;
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
		memset(&dn, 0, sizeof (dt_node_t));
		dn.dn_kind = DT_NODE_OP2;
		dn.dn_op = DT_TOK_DOT;
		dn.dn_left = dnp;
		dn.dn_right = &mn;

		memset(&mn, 0, sizeof (dt_node_t));
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
				instr = BPF_ALU64_REG(BPF_ADD, r2, r1);
				dt_irlist_append(dlp,
				    dt_cg_node_alloc(DT_LBL_NONE, instr));

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
	struct bpf_insn instr;
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
	instr = BPF_LOAD(BPF_DW, dnp->dn_reg, BPF_REG_FP,
			 base + dnp->dn_ident->di_id);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

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

		instr = BPF_BRANCH_IMM(BPF_JNE, dnp->dn_reg, 0, label);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

		dt_cg_setx(dlp, dnp->dn_reg, dt_node_type_size(dnp));
		instr = DIF_INSTR_ALLOCS(dnp->dn_reg, dnp->dn_reg);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

		dnp->dn_ident->di_flags |= DT_IDFLG_DIFW;
		instr = DIF_INSTR_STV(stvop, dnp->dn_ident->di_id, dnp->dn_reg);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

		instr = DIF_INSTR_LDV(op, dnp->dn_ident->di_id, dnp->dn_reg);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

		dt_irlist_append(dlp, dt_cg_node_alloc(label, BPF_NOP()));
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

	struct bpf_insn instr;
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
	instr = BPF_LOAD(BPF_DW, dnp->dn_reg, BPF_REG_FP,
			 base + dnp->dn_ident->di_id);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

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

	if ((size = dt_node_type_size(dnp)) == sizeof (uint64_t))
		return;

	assert(size < sizeof (uint64_t));
	n = sizeof (uint64_t) * NBBY - size * NBBY;

	instr = BPF_ALU64_IMM(BPF_LSH, dnp->dn_reg, n);
	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

	instr = BPF_ALU64_REG((dnp->dn_flags & DT_NF_SIGNED) ?
	    BPF_ARSH : BPF_RSH, dnp->dn_reg, n);

	dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
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

	struct bpf_insn instr;
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
		instr = BPF_ALU64_IMM(BPF_XOR, dnp->dn_reg, 0);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
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

		instr = BPF_NEG_REG(dnp->dn_reg);

		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		break;

	case DT_TOK_DEREF:
		dt_cg_node(dnp->dn_child, dlp, drp);
		dnp->dn_reg = dnp->dn_child->dn_reg;

		if (!(dnp->dn_flags & DT_NF_REF)) {
			uint_t ubit = dnp->dn_flags & DT_NF_USERLAND;

			/*
			 * Save and restore DT_NF_USERLAND across dt_cg_load():
			 * we need the sign bit from dnp and the user bit from
			 * dnp->dn_child in order to get the proper opcode.
			 */
			dnp->dn_flags |=
			    (dnp->dn_child->dn_flags & DT_NF_USERLAND);

			/* FIXME: Does not handled signed or userland */
			instr = BPF_LOAD(dt_cg_load(dnp, ctfp, dnp->dn_type),
					 dnp->dn_reg, dnp->dn_reg, 0);

			dnp->dn_flags &= ~DT_NF_USERLAND;
			dnp->dn_flags |= ubit;

			dt_irlist_append(dlp,
			    dt_cg_node_alloc(DT_LBL_NONE, instr));
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
				instr = BPF_MOV_REG(dnp->dn_reg,
						    dxp->dx_ident->di_id);
				dt_irlist_append(dlp,
				    dt_cg_node_alloc(DT_LBL_NONE, instr));
				op = DIF_OP_XLATE;
			} else
				op = DIF_OP_XLARG;

			instr = DIF_INSTR_XLATE(op, 0, dnp->dn_reg);
			dt_irlist_append(dlp,
			    dt_cg_node_alloc(DT_LBL_NONE, instr));

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
	case DT_TOK_DOT:
		assert(dnp->dn_right->dn_kind == DT_NODE_IDENT);
		dt_cg_node(dnp->dn_left, dlp, drp);

		/*
		 * Ensure that the lvalue is not the NULL pointer.
		 */
		instr = BPF_BRANCH_IMM(BPF_JEQ, dnp->dn_left->dn_reg, 0,
				       yypcb->pcb_exitlbl);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));

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
			instr = BPF_ALU64_IMM(BPF_ADD, dnp->dn_left->dn_reg,
					      m.ctm_offset / NBBY);

			dt_irlist_append(dlp,
			    dt_cg_node_alloc(DT_LBL_NONE, instr));
		}

		if (!(dnp->dn_flags & DT_NF_REF)) {
			uint_t ubit = dnp->dn_flags & DT_NF_USERLAND;

			/*
			 * Save and restore DT_NF_USERLAND across dt_cg_load():
			 * we need the sign bit from dnp and the user bit from
			 * dnp->dn_left in order to get the proper opcode.
			 */
			dnp->dn_flags |=
			    (dnp->dn_left->dn_flags & DT_NF_USERLAND);

			/* FIXME: Does not handle signed and userland */
			instr = BPF_LOAD(dt_cg_load(dnp, ctfp, m.ctm_type),
					 dnp->dn_left->dn_reg,
					 dnp->dn_left->dn_reg, 0);

			dnp->dn_flags &= ~DT_NF_USERLAND;
			dnp->dn_flags |= ubit;

			dt_irlist_append(dlp,
			    dt_cg_node_alloc(DT_LBL_NONE, instr));

			if (dnp->dn_flags & DT_NF_BITFIELD)
				dt_cg_field_get(dnp, dlp, drp, ctfp, &m);
		}

		dnp->dn_reg = dnp->dn_left->dn_reg;
		break;

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

#ifdef FIXME
		instr = BPF_LOAD(BPF_DW, dnp->dn_reg, BPF_REG_FP,
				 0x4000 + stroff);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
#else
		/*
		 * The string table will be loaded as value for the 0 element
		 * in the strtab BPF array map.  We use a function call to get
		 * the actual string:
		 *	get_string(stroff);
		 */
		if (dt_regset_xalloc_args(drp) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
		instr = BPF_MOV_IMM(BPF_REG_1, stroff);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		idp = dt_dlib_get_func(yypcb->pcb_hdl, "dt_get_string");
		assert(idp != NULL);
		instr = BPF_CALL_FUNC(idp->di_id);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		dlp->dl_last->di_extern = idp;
		instr = BPF_MOV_REG(dnp->dn_reg, BPF_REG_0);
		dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE, instr));
		dt_regset_free_args(drp);
#endif
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
			instr = BPF_MOV_REG(dnp->dn_reg, dnp->dn_ident->di_id);
			dt_irlist_append(dlp,
			    dt_cg_node_alloc(DT_LBL_NONE, instr));
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
if ((idp = dnp->dn_ident)->di_kind != DT_IDENT_FUNC)
  fprintf(stderr, "%s() - FUNC %s %s()\n", __func__, dt_idkind_name(idp->di_kind), idp->di_name);
#if 0
			if ((idp = dnp->dn_ident)->di_kind != DT_IDENT_FUNC) {
				dnerror(dnp, D_CG_EXPR, "%s %s( ) may not be "
				    "called from a D expression (D program "
				    "context required)\n",
				    dt_idkind_name(idp->di_kind), idp->di_name);
			}
#endif

			dt_cg_arglist(dnp->dn_ident, dnp->dn_args, dlp, drp);

			if ((dnp->dn_reg = dt_regset_alloc(drp)) == -1)
				longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

/* FIXME */
			instr = BPF_CALL_HELPER(-dnp->dn_ident->di_id);
			dt_irlist_append(dlp, dt_cg_node_alloc(DT_LBL_NONE,
					 instr));
			instr = BPF_MOV_REG(dnp->dn_reg, BPF_REG_0);
			dt_irlist_append(dlp,
			    dt_cg_node_alloc(DT_LBL_NONE, instr));

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
				instr = BPF_LOAD(dt_cg_load(dnp, ctfp,
							    dnp->dn_type),
						 dnp->dn_reg, dnp->dn_reg, 0);
				dt_irlist_append(dlp,
				    dt_cg_node_alloc(DT_LBL_NONE, instr));
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
		else {
			instr = BPF_MOV_IMM(dnp->dn_reg, dnp->dn_value);
			dt_irlist_append(dlp,
					 dt_cg_node_alloc(DT_LBL_NONE, instr));
		}
		break;

	default:
		xyerror(D_UNKNOWN, "internal error -- token type %u is not a "
		    "valid D compilation token\n", dnp->dn_op);
	}
}

void
dt_cg(dt_pcb_t *pcb, dt_node_t *dnp)
{
	struct bpf_insn	instr;
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

	assert(pcb->pcb_dret == NULL);
	pcb->pcb_dret = dnp;

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
		dt_irlist_t	*dlp = &pcb->pcb_ir;

		dt_cg_prologue(pcb, dnp->dn_pred);

		for (act = dnp->dn_acts; act != NULL; act = act->dn_list) {
			pcb->pcb_dret = act->dn_expr;

			if (act->dn_kind == DT_NODE_DFUNC) {
				const dt_cg_actdesc_t	*actdp;
				dt_ident_t		*idp;

				idp = act->dn_expr->dn_ident;
				actdp = &_dt_cg_actions[DT_ACT_IDX(idp->di_id)];
				if (actdp->fun)
					actdp->fun(pcb, act->dn_expr,
						   actdp->kind);
			} else {
				dt_cg_node(act->dn_expr, &pcb->pcb_ir,
					   pcb->pcb_regs);
				assert (pcb->pcb_dret->dn_reg != -1);
				dt_regset_free(pcb->pcb_regs,
					       pcb->pcb_dret->dn_reg);
			}
		}

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
