/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <sys/types.h>

#include <stdlib.h>
#include <setjmp.h>
#include <assert.h>
#include <errno.h>

#include <sys/socket.h>			/* needed for if_arp.h on OL7/x86 */
#include <linux/if_arp.h>		/* ARPHRD_ETHER ARPHRD_INFINIBAND */

#include <dt_impl.h>
#include <dt_aggregate.h>
#include <dt_dis.h>
#include <dt_dctx.h>
#include <dt_cg.h>
#include <dt_grammar.h>
#include <dt_module.h>
#include <dt_parser.h>
#include <dt_printf.h>
#include <dt_provider.h>
#include <dt_probe.h>
#include <dt_string.h>
#include <bpf_asm.h>

#define DT_IGNOR	-1		/* Ignore the value */
#define DT_ISIMM	0
#define DT_ISREG	1

static void dt_cg_node(dt_node_t *, dt_irlist_t *, dt_regset_t *);
static void dt_cg_prep_dvar(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp,
			    dt_ident_t *idp, int isstore);
static void dt_cg_check_notnull(dt_irlist_t *dlp, dt_regset_t *drp, int reg);

/*
 * Variants of store and load macros that take an actual data size and call
 * bpf_ldst_size(sz, store) to determine the instruction size specifier.
 */
#define BPF_STOREX(sz, dst, ofs, src)					\
		BPF_STORE(bpf_ldst_size(sz, 1), (dst), (ofs), (src))
#define BPF_STOREX_IMM(sz, dst, ofs, src)				\
		BPF_STORE_IMM(bpf_ldst_size(sz, 1), (dst), (ofs), (src))
#define BPF_LOADX(sz, dst, src, ofs)					\
		BPF_LOAD(bpf_ldst_size(sz, 0), (dst), (src), (ofs))

/*
 * Determine the BPF size specifier for the given data size.  If the given size
 * does not map to a valid BPF instruction size specifier, an internal error is
 * reported.
 */
uint_t
bpf_ldst_size(ssize_t size, int store)
{
	switch (size) {
	case 1:
		return BPF_B;
	case 2:
		return BPF_H;
	case 4:
		return BPF_W;
	case 8:
		return BPF_DW;
	default:
		xyerror(D_UNKNOWN, "internal error -- cg cannot %s "
			"size %ld when passed by value\n",
			store ? "store" : "load", (long)size);
	}
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
 * Determine the load size for the specified node and CTF type, and return the
 * equivalent BPF size specifier.
 */
uint_t
dt_cg_ldsize(dt_node_t *dnp, ctf_file_t *ctfp, ctf_id_t type, ssize_t *ret_size)
{
	ctf_encoding_t	e;
	ssize_t		size;

	/*
	 * If we're loading a bit-field, the size of our load is found by
	 * rounding cte_bits up to a byte boundary and then finding the
	 * nearest power of two to this value (see clp2(), above).
	 */
	if (dnp &&
	    (dnp->dn_flags & DT_NF_BITFIELD) &&
	    ctf_type_encoding(ctfp, type, &e) != CTF_ERR)
		size = clp2(P2ROUNDUP(e.cte_bits, NBBY) / NBBY);
	else
		size = ctf_type_size(ctfp, type);

	if (ret_size)
		*ret_size = size;

	return bpf_ldst_size(size, 0);
}

/*
 * Generate code to access a member of the DTrace context (or the DTrace
 * context itself if member is -1).
 */
static void
dt_cg_access_dctx(int reg, dt_irlist_t *dlp, dt_regset_t *drp, int member)
{
	int	ctxreg = reg;

	/*
	 * Regular programs get a pointer to the DTrace context from the stack.
	 * Trampolines have a pointer to the DTrace context in %r9.
	 */
	if (yypcb->pcb_root->dn_kind != DT_NODE_TRAMPOLINE) {
		emit(dlp,  BPF_LOAD(BPF_DW, ctxreg, BPF_REG_FP, DT_STK_DCTX));

		if (member == -1)
			return;
	} else if (member == -1) {		/* trampoline, no member */
		emit(dlp, BPF_MOV_REG(reg, BPF_REG_9));
		return;
	} else					/* trampoline, member */
		ctxreg = BPF_REG_9;

	emit(dlp, BPF_LOAD(BPF_DW, reg, ctxreg, member));
}

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
 *	%r9 contains a pointer to dctx
 */
void
dt_cg_tramp_prologue_act(dt_pcb_t *pcb, dt_activity_t act)
{
	dtrace_hdl_t	*dtp = pcb->pcb_hdl;
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	dt_regset_t	*drp = pcb->pcb_regs;
	dt_ident_t	*aggs = dt_dlib_get_map(dtp, "aggs");
	dt_ident_t	*mem = dt_dlib_get_map(dtp, "mem");
	dt_ident_t	*state = dt_dlib_get_map(dtp, "state");
	dt_ident_t	*prid = dt_dlib_get_var(pcb->pcb_hdl, "PRID");
	dt_ident_t	*ro_off = dt_dlib_get_var(dtp, "RODATA_OFF");
	uint_t		lbl_fast = pcb->pcb_fastlbl;
	uint_t		lbl_exit = pcb->pcb_exitlbl;

	assert(aggs != NULL);
	assert(mem != NULL);
	assert(state != NULL);
	assert(prid != NULL);
	assert(ro_off != NULL);

	/* Reserve %r7 and %r8. */
	dt_regset_xalloc(drp, BPF_REG_7);
	dt_regset_xalloc(drp, BPF_REG_8);

	/*
	 * On input, %r1 is the BPF context.
	 *
	 * int dt_dtrace(void *ctx)	//     (%r1 = pointer to BPF context)
	 * {
	 *	dt_dctx_t	dctx;
	 *	uint32_t	key;
	 *	uintptr_t	rc;
	 *	char		*buf, *mem;
	 *
	 *				// mov %r9, %fp
	 *				// add %r9, -DCTX_SIZE
	 *				//     (%r9 = pointer to DTrace context)
	 *				// mov %r8, %r1
	 *				//     (%r8 = pointer to BPF context)
	 *	dctx.ctx = ctx;		// stdw [%r9 + DCTX_CTX], %r8
	 */
	emit(dlp,  BPF_MOV_REG(BPF_REG_9, BPF_REG_FP));
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_9, -DCTX_SIZE));
	emit(dlp,  BPF_MOV_REG(BPF_REG_8, BPF_REG_1));
	emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_9, DCTX_CTX, BPF_REG_8));

	/*
	 *	key = DT_STATE_ACTIVITY;// stw [%r9 + DCTX_ACT],
	 *				//		DT_STATE_ACTIVITY
	 *	rc = bpf_map_lookup_elem(&state, &key);
	 *				// lddw %r1, &state
	 *				// mov %r2, %r9
	 *				// add %r2, DCTX_ACT
	 *				// call bpf_map_lookup_elem
	 *				//     (%r1 ... %r5 clobbered)
	 *				//     (%r0 = map value)
	 *	if (rc == 0)		// jeq %r0, 0, lbl_fast
	 *		goto fast;
	 *	if (*rc != act)		// ldw %r1, [%r0 + 0]
	 *		goto fast;	// jne %r1, act, lbl_fast
	 *
	 *	dctx.act = rc;		// stdw [%r9 + DCTX_ACT], %r0
	 */
	emit(dlp,  BPF_STORE_IMM(BPF_W, BPF_REG_9, DCTX_ACT, DT_STATE_ACTIVITY));
	dt_cg_xsetx(dlp, state, DT_LBL_NONE, BPF_REG_1, state->di_id);
	emit(dlp,  BPF_MOV_REG(BPF_REG_2, BPF_REG_9));
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, DCTX_ACT));
	emit(dlp,  BPF_CALL_HELPER(BPF_FUNC_map_lookup_elem));
	emit(dlp,  BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_0, 0, lbl_fast));
	emit(dlp,  BPF_LOAD(BPF_W, BPF_REG_1, BPF_REG_0, 0));
	emit(dlp,  BPF_BRANCH_IMM(BPF_JNE, BPF_REG_1, act, lbl_fast));
	emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_9, DCTX_ACT, BPF_REG_0));

	/*
	 *	key = 0;		// stw [%r9 + DCTX_BUF], 0
	 *	rc = bpf_map_lookup_elem(&mem, &key);
	 *				// lddw %r1, &mem
	 *				// mov %r2, %r9
	 *				// add %r2, DCTX_BUF
	 *				// call bpf_map_lookup_elem
	 *				//     (%r1 ... %r5 clobbered)
	 *				//     (%r0 = 'mem' BPF map value)
	 *	if (rc == 0)		// jeq %r0, 0, lbl_fast
	 *		goto fast;
	 *				//     (%r0 = map value)
	 *				//     (%r7 = pointer to dt_mstate_t)
	 *				// mov %r7, %r0
	 *				// ldw %r1, [%r7 + DMST_PRID]
	 *	if (dctx.mst->prid != 0)// jne %r1, 0, lbl_fast
	 *		goto exit;
	 *	dctx.mst = rc;		// stdw [%r9 + DCTX_MST], %r7
	 *	dctx.mst->prid = PRID;	// stw [%r7 + DMST_PRID], PRID
	 *	dctx.mst->syscall_errno = 0;
	 *				// stw [%r7 + DMST_ERRNO], 0
	 */
	emit(dlp,  BPF_STORE_IMM(BPF_W, BPF_REG_9, DCTX_BUF, 0));
	dt_cg_xsetx(dlp, mem, DT_LBL_NONE, BPF_REG_1, mem->di_id);
	emit(dlp,  BPF_MOV_REG(BPF_REG_2, BPF_REG_9));
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, DCTX_BUF));
	emit(dlp,  BPF_CALL_HELPER(BPF_FUNC_map_lookup_elem));
	emit(dlp,  BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_0, 0, lbl_fast));
	emit(dlp,  BPF_MOV_REG(BPF_REG_7, BPF_REG_0));
	emit(dlp,  BPF_LOAD(BPF_W, BPF_REG_1, BPF_REG_7, DMST_PRID));
	emit(dlp,  BPF_BRANCH_IMM(BPF_JNE, BPF_REG_1, 0, lbl_fast));
	emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_9, DCTX_MST, BPF_REG_7));
	emite(dlp, BPF_STORE_IMM(BPF_W, BPF_REG_7, DMST_PRID, -1), prid);
	emit(dlp,  BPF_STORE_IMM(BPF_W, BPF_REG_7, DMST_ERRNO, 0));

	/*
	 *	buf = rc + roundup(sizeof(dt_mstate_t), 8);
	 *				// add %r0, roundup(
	 *						sizeof(dt_mstate_t), 8)
	 *	dctx.buf = buf;		// stdw [%r9 + DCTX_BUF], %r0
	 */
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_0, roundup(sizeof(dt_mstate_t), 8)));
	emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_9, DCTX_BUF, BPF_REG_0));

	/*
	 *	mem = buf + roundup(dtp->dt_maxreclen, 8);
	 *				// add %r0, roundup(dtp->dt_maxreclen,
	 *	dctx.mem = mem;		// stdw [%r9 + DCTX_MEM], %r0
	 */
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_0, roundup(dtp->dt_maxreclen, 8)));
	emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_9, DCTX_MEM, BPF_REG_0));

	/*
	 * Store -1 to the strtok internal-state offset to indicate
	 * that strtok internal state is not yet initialized.
	 */
	emit(dlp,  BPF_STORE_IMM(BPF_DW, BPF_REG_0, DMEM_STRTOK(dtp), -1));

	/*
	 * Store pointer to BPF map "name" in the DTrace context field "fld" at
	 * "offset".
	 *
	 *	key = 0;		// stw [%r9 + offset], 0
	 *	rc = bpf_map_lookup_elem(&name, &key);
	 *				// lddw %r1, &name
	 *				// mov %r2, %r9
	 *				// add %r2, offset
	 *				// call bpf_map_lookup_elem
	 *				//     (%r1 ... %r5 clobbered)
	 *				//     (%r0 = name BPF map value)
	 *	if (rc == 0)		// jeq %r0, 0, lbl_exit
	 *		goto exit;
	 *				//     (%r0 = pointer to map value)
	 *	dctx.fld = rc;		// stdw [%r9 + offset], %r0
	 */
#define DT_CG_STORE_MAP_PTR(name, offset) \
	do { \
		dt_ident_t *idp = dt_dlib_get_map(dtp, name); \
		\
		assert(idp != NULL); \
		emit(dlp, BPF_STORE_IMM(BPF_W, BPF_REG_9, (offset), 0)); \
		\
		dt_cg_xsetx(dlp, idp, DT_LBL_NONE, BPF_REG_1, idp->di_id); \
		emit(dlp, BPF_MOV_REG(BPF_REG_2, BPF_REG_9)); \
		emit(dlp, BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, (offset))); \
		emit(dlp, BPF_CALL_HELPER(BPF_FUNC_map_lookup_elem)); \
		\
		emit(dlp, BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_0, 0, lbl_exit)); \
		\
		emit(dlp, BPF_STORE(BPF_DW, BPF_REG_9, (offset), BPF_REG_0)); \
	} while(0)

	DT_CG_STORE_MAP_PTR("strtab", DCTX_STRTAB);

	/*
	 * We know that a pointer to the strtab data is in %r0 (because that is
	 * where the DT_CG_STORE_MAP_PTR() macro left it, and we know that dctx
	 * is in %r9.  So, we can just add RODATA_OFF to %r0, and store that in
	 * [%r9 + DCTX_RODATA].
	 */
	emite(dlp, BPF_ALU64_IMM(BPF_ADD, BPF_REG_0, -1), ro_off);
	emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_9, DCTX_RODATA, BPF_REG_0));

	if (dtp->dt_options[DTRACEOPT_SCRATCHSIZE] > 0)
		DT_CG_STORE_MAP_PTR("scratchmem", DCTX_SCRATCHMEM);
	if (dt_idhash_datasize(dtp->dt_globals) > 0)
		DT_CG_STORE_MAP_PTR("gvars", DCTX_GVARS);
	if (dtp->dt_maxlvaralloc > 0)
		DT_CG_STORE_MAP_PTR("lvars", DCTX_LVARS);
#undef DT_CG_STORE_MAP_PTR

	/*
	 * Aggregation data is stored in a CPU-specific BPF map.  Populate
	 * dctx->agg with the map for the current CPU.
	 *
	 *	key = bpf_get_smp_processor_id()
	 *				// call bpf_get_smp_processor_id
	 *				//     (%r1 ... %r5 clobbered)
	 *				//     (%r0 = cpuid)
	 *				// stdw [%r9 + DCTX_AGG], %r0
	 *	rc = bpf_map_lookup_elem(&aggs, &key);
	 *				// lddw %r1, &aggs
	 *				// mov %r2, %r9
	 *				// add %r2, DCTX_AGG
	 *				// call bpf_map_lookup_elem
	 *				//     (%r1 ... %r5 clobbered)
	 *				//     (%r0 = 'aggs' BPF map value)
	 *	if (rc == 0)		// jeq %r0, 0, lbl_exit
	 *		goto exit;
	 *
	 *	dctx.aggs = rc;		// stdw [%r9 + DCTX_AGG], %r0
	 */
	if (dtp->dt_maxaggdsize > 0) {
		emit(dlp,  BPF_CALL_HELPER(BPF_FUNC_get_smp_processor_id));
		emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_9, DCTX_AGG, BPF_REG_0));
		dt_cg_xsetx(dlp, aggs, DT_LBL_NONE, BPF_REG_1, aggs->di_id);
		emit(dlp,  BPF_MOV_REG(BPF_REG_2, BPF_REG_9));
		emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, DCTX_AGG));
		emit(dlp,  BPF_CALL_HELPER(BPF_FUNC_map_lookup_elem));
		emit(dlp,  BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_0, 0, lbl_exit));
		emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_9, DCTX_AGG, BPF_REG_0));
	}
}

void
dt_cg_tramp_prologue_cpu(dt_pcb_t *pcb) {
	dt_cg_tramp_prologue_act(pcb, DT_ACTIVITY_ACTIVE);
}

void
dt_cg_tramp_prologue(dt_pcb_t *pcb)
{
	dtrace_hdl_t	*dtp = pcb->pcb_hdl;

	/* Check if we are on the specified CPU (if any). */
	if (dtp->dt_options[DTRACEOPT_CPU] != DTRACEOPT_UNSET) {
		dt_irlist_t	*dlp = &pcb->pcb_ir;

		emit(dlp,  BPF_MOV_REG(BPF_REG_8, BPF_REG_1));

		emit(dlp,  BPF_CALL_HELPER(BPF_FUNC_get_smp_processor_id));
		emit(dlp,  BPF_BRANCH_IMM(BPF_JNE, BPF_REG_0,
					  dtp->dt_options[DTRACEOPT_CPU],
					  pcb->pcb_fastlbl));

		emit(dlp,  BPF_MOV_REG(BPF_REG_1, BPF_REG_8));
	}

	/* Call the rest of the prologue code generation. */
	dt_cg_tramp_prologue_cpu(pcb);
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
 * Copy the content of a dt_pt_regs structure referenced by %r8 into the 'regs'
 * member of the machine state.
 *
 * The caller must ensure that %r7 and %r8 contain the values set by the
 * dt_cg_tramp_prologue*() functions.
 */
void
dt_cg_tramp_copy_regs(dt_pcb_t *pcb)
{
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	int		i;

	/*
	 *	dctx->mst->regs = *(dt_pt_regs *)%r8;
	 */
	for (i = 0; i < sizeof(dt_pt_regs); i += 8) {
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_8, i));
		emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_REGS + i, BPF_REG_0));
	}
}

/*
 * Copy arguments from a dt_pt_regs structure referenced by %r8.
 * If 'called' is nonzero, the registers are laid out as when inside the
 * function: if zero, they are laid out as at the call instruction, before the
 * function is called (as is done for e.g. usdt).
 *
 * The caller must ensure that %r7 and %r8 contain the values set by the
 * dt_cg_tramp_prologue*() functions.
 */
void
dt_cg_tramp_copy_args_from_regs(dt_pcb_t *pcb, int called)
{
	dtrace_hdl_t	*dtp = pcb->pcb_hdl;
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	int		i;

	/*
	 *	for (i = 0; i < PT_REGS_ARGC; i++)
	 *		dctx->mst->argv[i] = PT_REGS_ARGi((dt_pt_regs *)%r8);
	 *				// lddw %r0, [%r8 + PT_REGS_ARGi]
	 *				// stdw [%r7 + DMST_ARG(i)], %r0
	 */
	emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_8, PT_REGS_ARG0));
	emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(0), BPF_REG_0));
	emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_8, PT_REGS_ARG1));
	emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(1), BPF_REG_0));
	emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_8, PT_REGS_ARG2));
	emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(2), BPF_REG_0));
	emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_8, PT_REGS_ARG3));
	emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(3), BPF_REG_0));
	emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_8, PT_REGS_ARG4));
	emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(4), BPF_REG_0));
	emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_8, PT_REGS_ARG5));
	emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(5), BPF_REG_0));
#ifdef PT_REGS_ARG6
	emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_8, PT_REGS_ARG6));
	emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(6), BPF_REG_0));
#endif
#ifdef PT_REGS_ARG7
	emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_8, PT_REGS_ARG7));
	emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(7), BPF_REG_0));
#endif

	/*
	 * Generate code to read the remainder of the arguments from the stack
	 * (if possible).
	 *
	 *	for (i = PT_REGS_ARGC;
	 *	     i < ARRAY_SIZE(((dt_mstate_t *)0)->argv); i++) {
	 *		int		rc;
	 *		uint64_t	*sp;
	 *
	 *		sp = (uint64_t *)(((dt_pt_regs *)%r8)->sp;
	 *		rc = bpf_probe_read[_user](dctx->mst->argv[i],
	 *					   sizeof(uint64_t),
	 *					   &sp[i - PT_REGS_ARGC +
	 *						   (called ? PT_REGS_ARGSTKBASE : 0)]);
	 *				// mov %r1, %r7
	 *				// add %r1, DMST_ARG(i)
	 *				// mov %r2, sizeof(uint64_t)
	 *				// lddw %r3, [%r8 + PT_REGS_SP]
	 *				// add %r3, (i - PT_REGS_ARGC +
	 *						 (called ? PT_REGS_ARGSTKBASE : 0)) *
	 *					    sizeof(uint64_t)
	 *				// call bpf_probe_read[_user]
	 *		if (rc != 0)
	 *			goto lbl_ok:
	 *				// jeq %r0, 0, lbl_ok
	 *		dctx->mst->argv[i] = 0;
	 *				// stdw [%r7 + DMST_ARG(i)], 0
	 *
	 *	lbl_ok:			// lbl_ok:
	 *	}
	 *
	 */
	for (i = PT_REGS_ARGC; i < ARRAY_SIZE(((dt_mstate_t *)0)->argv); i++) {
		uint_t	lbl_ok = dt_irlist_label(dlp);

		emit(dlp,  BPF_MOV_REG(BPF_REG_1, BPF_REG_7));
		emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, DMST_ARG(i)));
		emit(dlp,  BPF_MOV_IMM(BPF_REG_2, sizeof(uint64_t)));
		emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_3, BPF_REG_8, PT_REGS_SP));
		emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_3, (i - PT_REGS_ARGC +
			    (called ? PT_REGS_ARGSTKBASE : 0)) * sizeof(uint64_t)));
		emit(dlp,  BPF_CALL_HELPER(dtp->dt_bpfhelper[BPF_FUNC_probe_read_user]));
		emit(dlp,  BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_0, 0, lbl_ok));
		emit(dlp,  BPF_STORE_IMM(BPF_DW, BPF_REG_7, DMST_ARG(i), 0));
		emitl(dlp, lbl_ok,
			   BPF_NOP());
	}
}

/*
 * For some providers, we have
 *   - arg0 = PC if kernel (0 otherwise)
 *   - arg1 = PC if user space (0 otherwise)
 *
 * So put the PC in both arg0 and arg1, test the PC, and then zero out
 * either arg0 or arg1, as apropriate.
 *
 * The caller must ensure that %r7 and %r8 contain the values set by the
 * dt_cg_tramp_prologue*() functions.
 */
void
dt_cg_tramp_copy_pc_from_regs(dt_pcb_t *pcb)
{
	dt_regset_t	*drp = pcb->pcb_regs;
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	uint_t		Luser = dt_irlist_label(dlp);
	uint_t		Ldone = dt_irlist_label(dlp);

	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	/* place the PC in %r3, arg0, and arg1 */
        emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_3, BPF_REG_8, PT_REGS_IP));
        emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(0), BPF_REG_3));
        emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(1), BPF_REG_3));

	/* check if the PC is kernel or user space */
#if 0
	if (pcb->pcb_hdl->dt_bpfhelper[BPF_FUNC_probe_read_kernel]
	    == BPF_FUNC_probe_read_kernel) {
#else
	/*
	 * We do not seem to be guaranteed for some kernels that
	 * probe_read_kernel(), even if it exists, will signal an error
	 * when reading user space addresses.  So, do not use the
	 * probe_read_kernel() mechanism for now.
	 */
	if (0) {
#endif
		/*
		 * Use probe_read_kernel() if it is really is probe_read_kernel().
		 * On older kernels, it does not exist and is aliased to something else.
		 */

		/* test just a single byte */
		emit(dlp,  BPF_MOV_IMM(BPF_REG_2, 1));

		/* safe to write to FP+DT_STK_SP_BASE, which becomes the clause stack */
		emit(dlp,  BPF_MOV_REG(BPF_REG_1, BPF_REG_FP));
		emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, DT_STK_SP_BASE));

		/* bpf_probe_read_kernel(%fp + DT_STK_SP, 1, PC) */
		dt_regset_xalloc(drp, BPF_REG_0);
		emit(dlp,  BPF_CALL_HELPER(BPF_FUNC_probe_read_kernel));

		/* if there was a problem, assume it was user space */
		emit(dlp,  BPF_BRANCH_IMM(BPF_JNE, BPF_REG_0, 0, Luser));
		dt_regset_free(drp, BPF_REG_0);
	} else {
		/*
		 * If no real probe_read_kernel() exists, just test the highest bit.
		 */

		/* if the highest bit is 0, assume it was user space */
		emit(dlp, BPF_ALU64_IMM(BPF_RSH, BPF_REG_3, 63));
		emit(dlp,  BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_3, 0, Luser));
	}

	/* PC is kernel space (zero out arg1) */
	emit(dlp,  BPF_STORE_IMM(BPF_DW, BPF_REG_7, DMST_ARG(1), 0));
	emit(dlp,  BPF_JUMP(Ldone));

	/* PC is user space (zero out arg0) */
	emitl(dlp, Luser,
		   BPF_STORE_IMM(BPF_DW, BPF_REG_7, DMST_ARG(0), 0));

	/* done */
	emitl(dlp, Ldone,
	      BPF_NOP());
	dt_regset_free_args(drp);
}

/*
 * Copy return value from a dt_pt_regs structure referenced by %r8 to
 * mst->arg[1].  Zero the other args.
 *
 * The caller must ensure that %r7 and %r8 contain the values set by the
 * dt_cg_tramp_prologue*() functions.
 */
void
dt_cg_tramp_copy_rval_from_regs(dt_pcb_t *pcb)
{
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	int		i;

	emit(dlp, BPF_STORE_IMM(BPF_DW, BPF_REG_7, DMST_ARG(0), 0));

	emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_8, PT_REGS_RET));
	emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(1), BPF_REG_0));

	for (i = 2; i < ARRAY_SIZE(((dt_mstate_t *)0)->argv); i++)
		emit(dlp, BPF_STORE_IMM(BPF_DW, BPF_REG_7, DMST_ARG(i), 0));
}

static dt_node_t *
dt_cg_tramp_var(const char *name)
{
	char		*pf, *nm;
	dt_node_t	*dnp;

	pf = strdup(name);
	nm = strstr(pf, "->");
	if (nm != NULL) {
		dt_node_t	*n1, *n2;

		*nm = '\0';
		if (strcmp(pf, "this") != 0 && strcmp(pf, "self") != 0)
			xyerror(D_VAR_UNSUP, "%s: invalid variable class\n",
				pf);

		nm += 2;
		n1 = dt_node_ident(pf);
		n2 = dt_node_ident(strdup(nm));
		dnp = dt_node_op2(DT_TOK_PTR, n1, n2);
	} else
		dnp = dt_node_ident(pf);

	dnp = dt_node_cook(dnp, DT_IDFLG_REF);
	dt_ident_set_storage(dnp->dn_ident,
			     ctf_type_align(dnp->dn_ctfp, dnp->dn_type),
			     ctf_type_size(dnp->dn_ctfp, dnp->dn_type));

	return dnp;
}

void
dt_cg_tramp_decl_var(dt_pcb_t *pcb, dt_ident_t *idp)
{
	dtrace_hdl_t		*dtp = pcb->pcb_hdl;
	dt_idhash_t		*dhp;
	uint_t			id;
	dtrace_typeinfo_t	dtt;


	if (idp->di_flags & DT_IDFLG_LOCAL)
		dhp = pcb->pcb_locals;
	else if (idp->di_flags & DT_IDFLG_TLS)
		dhp = dtp->dt_tls;
	else
		dhp = dtp->dt_globals;

	if (dt_idhash_lookup(dhp, idp->di_name))
		return;

	if (dt_idhash_nextid(dhp, &id) == -1)
		xyerror(D_ID_OFLOW, "cannot create %s: limit on number of "
				    "%s variables exceeded\n", idp->di_name,
				    dt_idhash_name(dhp));

	if (dt_idhash_insert(dhp, idp->di_name, idp->di_kind, idp->di_flags,
			     id, idp->di_attr, idp->di_vers,
			     idp->di_ops ? idp->di_ops : &dt_idops_thaw,
			     idp->di_iarg, 0) == NULL)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOMEM);

	idp = dt_idhash_lookup(dhp, idp->di_name);
	assert(idp != NULL);
	if (dtrace_lookup_by_type(dtp, DTRACE_OBJ_EVERY, idp->di_iarg,
				  &dtt) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_CTF);

	dt_ident_type_assign(idp, dtt.dtt_ctfp, dtt.dtt_type);
}

/*
 * Generate code to store the value (or address) of the named variable in
 * register 'reg'.
 * If isstore is true, the variable will be prepared for storing data.  This
 * means that if the variable is dynamic, it will be created if it does not
 * exist yet.
 */
void
dt_cg_tramp_get_var(dt_pcb_t *pcb, const char *name, int isstore, int reg)
{
	dt_node_t	*dnp;
	dt_ident_t	*idp;
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	dt_regset_t	*drp = pcb->pcb_regs;

	dnp = dt_cg_tramp_var(name);
	idp = dnp->dn_ident;

	/* Retrieving a variable value or address. */
	if (!isstore) {
		dt_cg_node(dnp, dlp, drp);
		goto out;
	}

	/*
	 * Getting a variable for storing a value to it (i.e. preparing it for
	 * writing) is more complex because for dynamic variables we may need
	 * to actually create the variable.  For non-dynamic variables, we
	 * will always return the address of the variable (and perform a NULL
	 * check) so the trampoline code can perform the actual store.
	 */
	if (idp->di_kind == DT_IDENT_ARRAY || idp->di_flags & DT_IDFLG_TLS) {
		/*
		 * We do not call dt_cg_node() for dnp, which means dnp->dn_reg
		 * will be -1.  Set it to be %r0 because dt_cg_prep_dvar() will
		 * store the var address in that register.  We need to mark the
		 * identifier as written to.
		 */
		idp->di_flags |= DT_IDFLG_DIFW;
		dnp->dn_reg = BPF_REG_0;
		dt_cg_prep_dvar(dnp, dlp, drp, idp, 1);
		emit(dlp, BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_0, 0, pcb->pcb_exitlbl));
	} else {
		dnp->dn_flags |= DT_NF_REF;

		dt_cg_node(dnp, dlp, drp);
		dt_cg_check_notnull(dlp, drp, dnp->dn_reg);
	}

out:
	if (dnp->dn_reg != reg)
		emit(dlp, BPF_MOV_REG(reg, dnp->dn_reg));

	dt_regset_free(drp, dnp->dn_reg);
}

/*
 * Generate code to delete the named dynamic variable.  The variable must be
 * a TLS variable.
 */
void
dt_cg_tramp_del_var(dt_pcb_t *pcb, const char *name)
{
	dt_node_t	*dnp, *znp;
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	dt_regset_t	*drp = pcb->pcb_regs;

	dnp = dt_cg_tramp_var(name);

	if (!(dnp->dn_ident->di_flags & DT_IDFLG_TLS))
		dnerror(dnp, D_VAR_UNSUP, "%s is not a TLS variable\n", name);

	znp = dt_node_int(0);
	dnp = dt_node_op2(DT_TOK_ASGN, dnp, znp);
	dnp = dt_node_cook(dnp, DT_IDFLG_MOD);

	dt_cg_node(dnp, dlp, drp);
	dt_regset_free(drp, dnp->dn_reg);
}

/*
 * Save the probe arguments.
 */
void
dt_cg_tramp_save_args(dt_pcb_t *pcb)
{
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	int		i;

	for (i = 0; i < ARRAY_SIZE(((dt_mstate_t *)0)->argv); i++) {
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_7, DMST_ARG(i)));
		emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ORIG_ARG(i), BPF_REG_0));
	}
}

/*
 * Restore the saved probe arguments.
 */
void
dt_cg_tramp_restore_args(dt_pcb_t *pcb)
{
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	int		i;

	for (i = 0; i < ARRAY_SIZE(((dt_mstate_t *)0)->argv); i++) {
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_7, DMST_ORIG_ARG(i)));
		emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(i), BPF_REG_0));
	}
}

/*
 * Populate the probe arguments based on the provided dt_argdesc_t array.  The
 * caller must save the arguments because argument mapping copies values from
 * the saved arguments to the current arguments.  After this function returns,
 * the caller should adjust the mapping to reflect that shuffling has been done.
 *
 * The caller must ensure that %r7 contains the value set by the
 * dt_cg_tramp_prologue*() functions.
 */
void
dt_cg_tramp_map_args(dt_pcb_t *pcb, dt_argdesc_t *args, size_t nargs)
{
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	int		i;

	for (i = 0; i < nargs; i++) {
		if (args[i].mapping != i) {
			emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_7, DMST_ORIG_ARG(args[i].mapping)));
			emit(dlp, BPF_STORE(BPF_DW, BPF_REG_7, DMST_ARG(i), BPF_REG_0));
		}
	}
}

typedef struct {
	dt_irlist_t	*dlp;
	dt_activity_t	act;
	uint_t		lbl_exit;
} dt_clause_arg_t;

static int
dt_cg_call_clause(dtrace_hdl_t *dtp, dtrace_stmtdesc_t *sdp, dt_clause_arg_t *arg)
{
	dt_irlist_t	*dlp = arg->dlp;
	dt_ident_t	*idp = sdp->dtsd_clause;

	/*
	 *	if (*dctx.act != act)	// ldw %r0, [%r9 + DCTX_ACT]
	 *		goto exit;	// ldw %r0, [%r0 + 0]
	 *				// jne %r0, act, lbl_exit
	 */
	emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_9, DCTX_ACT));
	emit(dlp,  BPF_LOAD(BPF_W, BPF_REG_0, BPF_REG_0, 0));
	emit(dlp,  BPF_BRANCH_IMM(BPF_JNE, BPF_REG_0, arg->act, arg->lbl_exit));

	/*
	 *	dctx.mst->scratch_top = 8;
	 *				// stw [%r7 + DMST_SCRATCH_TOP], 8
	 */
	emit(dlp,  BPF_STORE_IMM(BPF_W, BPF_REG_7, DMST_SCRATCH_TOP, 8));

	/*
	 *	dt_clause(dctx);	// mov %r1, %r9
	 *				// call clause
	 */
	emit(dlp,  BPF_MOV_REG(BPF_REG_1, BPF_REG_9));
	emite(dlp, BPF_CALL_FUNC(idp->di_id), idp);

	return 0;
}

void
dt_cg_tramp_call_clauses(dt_pcb_t *pcb, const dt_probe_t *prp, dt_activity_t act)
{
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	dt_clause_arg_t	arg = { dlp, act, pcb->pcb_exitlbl };

	dt_probe_stmt_iter(pcb->pcb_hdl, prp, (dt_stmt_f *)dt_cg_call_clause, &arg);
}

static int
dt_cg_add_dependent(dtrace_hdl_t *dtp, dt_probe_t *prp, void *arg)
{
	dt_pcb_t	*pcb = dtp->dt_pcb;
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	dt_ident_t	*idp = dt_dlib_add_probe_var(pcb->pcb_hdl, prp);
	uint_t		exitlbl = dt_irlist_label(dlp);
	int		skip = 0;

	dt_cg_tramp_save_args(pcb);
	pcb->pcb_parent_probe = pcb->pcb_probe;
	pcb->pcb_probe = prp;
	emite(dlp, BPF_STORE_IMM(BPF_W, BPF_REG_7, DMST_PRID, prp->desc->id), idp);
	if (prp->prov->impl->trampoline != NULL)
		skip = prp->prov->impl->trampoline(pcb, exitlbl);

	if (!skip)
		dt_cg_tramp_call_clauses(pcb, prp, DT_ACTIVITY_ACTIVE);

	emitl(dlp, exitlbl,
		   BPF_NOP());

	pcb->pcb_probe = pcb->pcb_parent_probe;
	pcb->pcb_parent_probe = NULL;
	dt_cg_tramp_restore_args(pcb);

	return 0;
}

static void
dt_cg_tramp_add_dependents(dt_pcb_t *pcb, const dt_probe_t *prp)
{
	dt_probe_dependent_iter(pcb->pcb_hdl, prp, dt_cg_add_dependent, NULL);
}

void
dt_cg_tramp_return(dt_pcb_t *pcb)
{
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	dt_regset_t	*drp = pcb->pcb_regs;

	/*
	 * exit:
	 *				// lddw %r9, [%fp-DCTX_SIZE+DCTX_MST]
	 *	dctx.mst->prid = 0;	// stw [%r9 + DMST_PRID], 0
	 * fast:
	 *	return 0;		// mov %r0, 0
	 *				// exit
	 * }
	 */
	emitl(dlp, pcb->pcb_exitlbl,
		   BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_FP, (ushort_t)-DCTX_SIZE+DCTX_MST));
	emit(dlp,  BPF_STORE_IMM(BPF_W, BPF_REG_0, DMST_PRID, 0));
	emitl(dlp, pcb->pcb_fastlbl,
		   BPF_MOV_IMM(BPF_REG_0, 0));
	emit(dlp,  BPF_RETURN());

	/* Free %r7 and %r8. */
	dt_regset_free(drp, BPF_REG_7);
	dt_regset_free(drp, BPF_REG_8);
}

void
dt_cg_tramp_epilogue(dt_pcb_t *pcb)
{
	dt_cg_tramp_call_clauses(pcb, pcb->pcb_probe, DT_ACTIVITY_ACTIVE);

	/*
	 * For each dependent probe (if any):
	 *	- Call dt_cg_tramp_save_args()
	 *	- Set PRID to the probe ID of the dependent probe
	 *	- Call prp->prov->impl->trampoline()
	 *		[ This will generate the pseudo-trampoline that sets
	 *		  up the arguments for the dependent probe, possibly
	 *		  based on the arguments of the underllying probe. ]
	 *	- Call dt_cg_tramp_call_clauses() for the dependent probe
	 *	- Call dt_cg_tramp_restore_args()
	 *
	 * Possible optimization:
	 *	Do not call dt_cg_tramp_restore_args() after the last dependent
	 *	probe.
	 */
	dt_cg_tramp_add_dependents(pcb, pcb->pcb_probe);
	dt_cg_tramp_return(pcb);
}

void
dt_cg_tramp_epilogue_advance(dt_pcb_t *pcb, dt_activity_t act)
{
	dt_irlist_t	*dlp = &pcb->pcb_ir;

	dt_cg_tramp_call_clauses(pcb, pcb->pcb_probe, act);

	/*
	 *	(*dctx.act)++;		// lddw %r0, [%r9 + DCTX_ACT)]
	 *				// mov %r1, 1
	 *				// xadd [%r0 + 0], %r1
	 */
	emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_9, DCTX_ACT));
	emit(dlp, BPF_MOV_IMM(BPF_REG_1, 1));
	emit(dlp, BPF_XADD_REG(BPF_W, BPF_REG_0, 0, BPF_REG_1));

	dt_cg_tramp_return(pcb);
}

static int
dt_cg_tramp_error_call_clause(dtrace_hdl_t *dtp, dtrace_stmtdesc_t *sdp, dt_irlist_t *dlp)
{
	dt_ident_t *idp = sdp->dtsd_clause;

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

	dt_probe_stmt_iter(dtp, dtp->dt_error, (dt_stmt_f *)dt_cg_tramp_error_call_clause, dlp);

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
 *	- Store the base pointer to the output data buffer in %r9.
 *	- Initialize the machine state (dctx->mst).
 *	- Store 0 to indicate no active speculation at [%r9 + DBUF_SPECID].
 *	- Store the prid at [%r9 + DBUF_PRID].
 *	- Store the stid at [%r9 + DBUF_STID].
 *	- Evaluate the predicate expression and return if false.
 *
 * The dt_program() function will always return 0.
 */
static void
dt_cg_prologue(dt_pcb_t *pcb, dt_node_t *pred)
{
	dt_irlist_t	*dlp = &pcb->pcb_ir;

	/*
	 * void dt_program(dt_dctx_t *dctx)
	 *				//     (%r1 = pointer to dt_dctx_t)
	 * {
	 *	int	rc;
	 *	char	*buf;		//     (%r9 = reserved reg for buf)
	 *
	 *				// stdw [%fp + DT_STK_DCTX], %r1
	 *				// mov %r0, %fp
	 *				// add %r0, DT_STK_SP_BASE
	 *				// stdw [%fp + DT_STK_SP], %r0
	 */
	TRACE_REGSET("Prologue: Begin");
	emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_FP, DT_STK_DCTX, BPF_REG_1));
	emit(dlp,  BPF_MOV_REG(BPF_REG_0, BPF_REG_FP));
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_0, DT_STK_SP_BASE));
	emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_FP, DT_STK_SP, BPF_REG_0));

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
	 *	dctx->mst->specsize = 0;// stdw [%r0 + DMST_SPECSIZE], 0
	 *	dctx->mst->stid = STID;	// stw [%r0 + DMST_STID], STID
	 */
	emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_0, DCTX_MST));
	emit(dlp,  BPF_STORE_IMM(BPF_DW, BPF_REG_0, DMST_FAULT, 0));
	emit(dlp,  BPF_STORE_IMM(BPF_DW, BPF_REG_0, DMST_TSTAMP, 0));
	emit(dlp,  BPF_STORE_IMM(BPF_DW, BPF_REG_0, DMST_SPECSIZE, 0));
	emit(dlp,  BPF_STORE_IMM(BPF_W, BPF_REG_0, DMST_STID, pcb->pcb_stmt->dtsd_id));

	/*
	 *	Set the speculation ID field to zero to indicate no active
	 *	speculation.
	 *	*((uint32_t *)&buf[DBUF_SPECID]) = 0;
	 *				// stw [%r9 + DBUF_SPECID], 0
	 */
	emit(dlp,  BPF_STORE_IMM(BPF_W, BPF_REG_9, DBUF_SPECID, 0));

	/*
	 *	*((uint32_t *)&buf[DBUF_PRID]) = dctx->mst->prid;
	 *				// ld %r1, [%r0 + DMST_PRID]
	 *				// st [%r9 + DBUF_PRID], %r1
	 *	*((uint32_t *)&buf[DBUF_STID]) = stid;
	 *				// st [%r9 + DBUF_STID], stid
	 */
	emit (dlp, BPF_LOAD(BPF_W, BPF_REG_1, BPF_REG_0, DMST_PRID));
	emit (dlp, BPF_STORE(BPF_W, BPF_REG_9, DBUF_PRID, BPF_REG_1));
	emit (dlp, BPF_STORE_IMM(BPF_W, BPF_REG_9, DBUF_STID, pcb->pcb_stmt->dtsd_id));

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
	 * Set the offset for the beginning of trace data.
	 */
	pcb->pcb_bufoff = DBUF_DATA;
}

/*
 * Generate the function epilogue:
 *	- Submit the buffer to the perf event output buffer for the current
 *	  cpu, if this is a data recording action..
 *	- Return 0
 * }
 */
static void
dt_cg_epilogue(dt_pcb_t *pcb)
{
	dtrace_hdl_t	*dtp = pcb->pcb_hdl;
	dt_irlist_t	*dlp = &pcb->pcb_ir;

	TRACE_REGSET("Epilogue: Begin");

	/*
	 * Output the buffer if:
	 *   - data-recording action, or
	 *   - default action (no clause specified)
	 *   - committing or discarding a speculation
	 */
	if (pcb->pcb_stmt->dtsd_clauseflags & DT_CLSFLAG_DATAREC ||
	    pcb->pcb_stmt->dtsd_clauseflags & DT_CLSFLAG_COMMIT_DISCARD) {
		dt_ident_t	*buffers = dt_dlib_get_map(dtp, "buffers");
		dt_ident_t	*idp;
		int		cflags = pcb->pcb_stmt->dtsd_clauseflags;

		assert(buffers != NULL);

		/*
		 *	rc = bpf_perf_event_output(dctx->ctx, &buffers,
		 *				   BPF_F_CURRENT_CPU,
		 *				   buf + 4, bufoff - 4);
		 *				// lddw %r1, [%fp + DT_STK_DCTX]
		 *				// lddw %r1, [%r1 + DCTX_CTX]
		 *				// lddw %r2, &buffers
		 *				// lddw %r3, BPF_F_CURRENT_CPU
		 *				// mov %r4, %r9
		 *				// add %r4, 4
		 *				// mov %r5, pcb->pcb_bufoff
		 *				// add %r5, -4
		 *				// call bpf_perf_event_output
		 */
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_1, BPF_REG_FP, DT_STK_DCTX));
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_1, BPF_REG_1, DCTX_CTX));
		dt_cg_xsetx(dlp, buffers, DT_LBL_NONE, BPF_REG_2, buffers->di_id);
		dt_cg_xsetx(dlp, NULL, DT_LBL_NONE, BPF_REG_3, BPF_F_CURRENT_CPU);
		emit(dlp, BPF_MOV_REG(BPF_REG_4, BPF_REG_9));
		emit(dlp, BPF_ALU64_IMM(BPF_ADD, BPF_REG_4, 4));
		emit(dlp, BPF_MOV_IMM(BPF_REG_5, pcb->pcb_bufoff));
		emit(dlp, BPF_ALU64_IMM(BPF_ADD, BPF_REG_5, -4));
		emit(dlp, BPF_CALL_HELPER(BPF_FUNC_perf_event_output));

		/*
		 * If writing the trace data to the output buffer failed,
		 * increment the drop count for speculation buffers
		 * (speculate() in the clause) or for the principal buffer
		 * (no speculate() in the clause).
		 */
		emit(dlp, BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_0, 0, pcb->pcb_exitlbl));
		if (cflags & DT_CLSFLAG_SPECULATE) {
			idp = dt_dlib_get_map(dtp, "state");
			assert(idp != NULL);
			dt_cg_xsetx(dlp, idp, DT_LBL_NONE, BPF_REG_1, idp->di_id);
			emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_2, BPF_REG_FP, DT_STK_SP));
			emit(dlp, BPF_STORE_IMM(BPF_DW, BPF_REG_2, 0, DT_STATE_SPEC_DROPS));
			emit(dlp, BPF_CALL_HELPER(BPF_FUNC_map_lookup_elem));
			emit(dlp, BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_0, 0, pcb->pcb_exitlbl));
			emit(dlp, BPF_MOV_IMM(BPF_REG_1, 1));
			emit(dlp, BPF_XADD_REG(BPF_W, BPF_REG_0, 0, BPF_REG_1));

			/*
			 * Add the size of the trace data to the overall count
			 * for the current speculation.
			 */
			idp = dt_dlib_get_map(dtp, "specs");
			assert(idp != NULL);
			dt_cg_xsetx(dlp, idp, DT_LBL_NONE, BPF_REG_1, idp->di_id);
			emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_2, BPF_REG_FP, DT_STK_SP));
			emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_9, DBUF_SPECID));
			emit(dlp, BPF_STORE(BPF_DW, BPF_REG_2, 0, BPF_REG_0));
			emit(dlp, BPF_CALL_HELPER(BPF_FUNC_map_lookup_elem));
			emit(dlp, BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_0, 0, pcb->pcb_exitlbl));
			emit(dlp, BPF_MOV_IMM(BPF_REG_1, pcb->pcb_bufoff));
			emit(dlp, BPF_XADD_REG(BPF_DW, BPF_REG_0, offsetof(dt_bpf_specs_t, size), BPF_REG_1));
		} else {
			idp = dt_dlib_get_map(dtp, "cpuinfo");
			assert(idp != NULL);
			dt_cg_xsetx(dlp, idp, DT_LBL_NONE, BPF_REG_1, idp->di_id);
			emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_2, BPF_REG_FP, DT_STK_SP));
			emit(dlp, BPF_STORE_IMM(BPF_DW, BPF_REG_2, 0, 0));
			emit(dlp, BPF_CALL_HELPER(BPF_FUNC_map_lookup_elem));
			emit(dlp, BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_0, 0, pcb->pcb_exitlbl));
			emit(dlp, BPF_MOV_IMM(BPF_REG_1, 1));
			emit(dlp, BPF_XADD_REG(BPF_DW, BPF_REG_0, offsetof(dt_bpf_cpuinfo_t, buf_drops), BPF_REG_1));
		}
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
 * set the fault information.  ILL may be an absolute value or a register,
 * depending on the value of ILLISREG.
 *
 * Doesn't use the regset code on the grounds that when this is executed we will
 * never reuse any of the callers' regs in any case.
 */
static void
dt_cg_probe_error(dt_pcb_t *pcb, uint32_t fault, int illisreg, uint64_t ill)
{
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	dt_regset_t	*drp = pcb->pcb_regs;
	dt_ident_t	*idp;

	/*
	 *	dt_probe_error(
	 *		dctx,		// lddw %r1, %fp, DT_STK_DCTX
	 *		PC,		// mov %r2, PC
	 *		fault,		// mov %r3, fault
	 *		illval);	// mov %r4, illval
	 *				// call dt_probe_error
	 *	return;			// exit
	 */

	/*
	 * Move the only pre-existing reg we need (ill) into place first,
	 * since we don't know which reg it is and it might perfectly well be
	 * one we are about to blindly reuse.
	 */

	if (illisreg == DT_ISREG && ill != 4)
		emit(dlp, BPF_MOV_REG(BPF_REG_4, ill));
	else
		emit(dlp, BPF_MOV_IMM(BPF_REG_4, ill));

	dt_cg_access_dctx(BPF_REG_1, dlp, drp, -1);
	idp = dt_dlib_get_var(pcb->pcb_hdl, "PC");
	assert(idp != NULL);
	emite(dlp, BPF_MOV_IMM(BPF_REG_2, -1), idp);
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, 3));
	emit(dlp,  BPF_MOV_IMM(BPF_REG_3, fault));
	idp = dt_dlib_get_func(pcb->pcb_hdl, "dt_probe_error");
	assert(idp != NULL);
	emite(dlp, BPF_CALL_FUNC(idp->di_id), idp);
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
	dt_cg_probe_error(yypcb, DTRACEFLT_BADADDR, DT_ISIMM, 0);
	emitl(dlp, lbl_notnull,
		   BPF_NOP());
}

/*
 * Check whether mst->fault indicates a fault was triggered.  If so, abort the
 * current clause by means of a straight jump to the exit label.
 */
static void
dt_cg_check_fault(dt_pcb_t *pcb)
{
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	dt_regset_t	*drp = pcb->pcb_regs;
	int		reg = dt_regset_alloc(drp);

	if (reg == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	dt_cg_access_dctx(reg, dlp, drp, DCTX_MST);
	emit(dlp,  BPF_LOAD(BPF_DW, reg, reg, DMST_FAULT));
	emit(dlp,  BPF_BRANCH_IMM(BPF_JNE, reg, 0, pcb->pcb_exitlbl));
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
	uint_t		lbl_ok = dt_irlist_label(dlp);

	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	emit(dlp, BPF_MOV_REG(BPF_REG_1, dst));
	emit(dlp, BPF_MOV_IMM(BPF_REG_2, size));
	emit(dlp, BPF_MOV_REG(BPF_REG_3, src));
	dt_regset_xalloc(drp, BPF_REG_0);
	emit(dlp, BPF_CALL_HELPER(BPF_FUNC_probe_read));
	dt_regset_free_args(drp);

	emit(dlp,  BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_0, 0, lbl_ok));
	dt_cg_probe_error(yypcb, DTRACEFLT_BADADDR, DT_ISREG, src);
	emitl(dlp, lbl_ok,
	      BPF_NOP());

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

/*
 * Initialize the temporary string offsets and mark all not in use.
 */
static void
dt_cg_tstring_reset(dtrace_hdl_t *dtp)
{
	int		i;
	dt_tstring_t	*ts;
	uint64_t	size = roundup(dtp->dt_options[DTRACEOPT_STRSIZE] + 1,
				       8);

	if (dtp->dt_tstrings == NULL) {
		dtp->dt_tstrings = dt_calloc(dtp, DT_TSTRING_SLOTS,
					     sizeof(dt_tstring_t));
		if (dtp->dt_tstrings == NULL)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOMEM);

		ts = dtp->dt_tstrings;
		for (i = 0; i < DT_TSTRING_SLOTS; i++, ts++)
			ts->offset = i * size;
	}

	ts = dtp->dt_tstrings;
	for (i = 0; i < DT_TSTRING_SLOTS; i++, ts++)
		ts->in_use = 0;
}

/*
 * Allocate a temporary string from the pool.  This function returns the offset
 * of the allocated temporary string within scratch memory (base: dctx->mem).
 */
static uint64_t
dt_cg_tstring_xalloc(dt_pcb_t *pcb)
{
	int		i;
	dt_tstring_t	*ts = pcb->pcb_hdl->dt_tstrings;

	for (i = 0; i < DT_TSTRING_SLOTS; i++, ts++) {
		if (!ts->in_use)
			break;
	}

	assert(i < DT_TSTRING_SLOTS);
	ts->in_use = 1;
	return ts->offset;
}

/*
 * Associate a temporary string with the given node.
 */
static void
dt_cg_tstring_alloc(dt_pcb_t *pcb, dt_node_t *dnp)
{
	dt_node_tstring(dnp, dt_cg_tstring_xalloc(pcb));
}

/*
 * Release the temporary string associated with the given offset.
 */
static void
dt_cg_tstring_xfree(dt_pcb_t *pcb, uint64_t offset)
{
	int		i;
	dt_tstring_t	*ts = pcb->pcb_hdl->dt_tstrings;

	for (i = 0; i < DT_TSTRING_SLOTS; i++, ts++) {
		if (ts->offset == offset)
			break;
	}

	assert(i < DT_TSTRING_SLOTS);
	assert(ts->in_use != 0);

	ts->in_use = 0;
}

/*
 * Release the temporary string associated with the given node.
 */
static void
dt_cg_tstring_free(dt_pcb_t *pcb, dt_node_t *dnp)
{
	switch (dnp->dn_kind) {
	case DT_NODE_FUNC:
	case DT_NODE_OP1:
	case DT_NODE_OP2:
	case DT_NODE_OP3:
	case DT_NODE_DEXPR:
		if (dnp->dn_tstring) {
			dt_cg_tstring_xfree(pcb, dnp->dn_tstring->dn_value);
			dnp->dn_tstring = NULL;
		}
	}
}

/*
 * Validate sized access from an alloca pointer value.
 *
 * pos + size < top <=> pos < top - size
 */
static void
dt_cg_alloca_access_check(dt_irlist_t *dlp, dt_regset_t *drp, int reg,
			  int isreg, ssize_t size)
{
	int	scratchsize = yypcb->pcb_hdl->dt_options[DTRACEOPT_SCRATCHSIZE];
	uint_t	lbl_illval = dt_irlist_label(dlp);
	uint_t	lbl_base_ok = dt_irlist_label(dlp);
	uint_t	lbl_ok = dt_irlist_label(dlp);

	dt_regset_xalloc(drp, BPF_REG_0);
	dt_cg_access_dctx(BPF_REG_0, dlp, drp, DCTX_MST);
	emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_0, DMST_SCRATCH_TOP));

	emit(dlp,  BPF_BRANCH_IMM(BPF_JSLT, reg, 8, lbl_illval));
	emit(dlp,  BPF_BRANCH_IMM(BPF_JSGE, reg, scratchsize, lbl_illval));
	emit(dlp,  BPF_BRANCH_REG(BPF_JLT, reg, BPF_REG_0, lbl_base_ok));
	emitl(dlp, lbl_illval,
		   BPF_NOP());
	dt_cg_probe_error(yypcb, DTRACEFLT_BADADDR, DT_ISREG, reg);

	emitl(dlp, lbl_base_ok,
		   BPF_NOP());

	if (isreg == DT_ISREG)
		emit(dlp, BPF_ALU64_REG(BPF_SUB, BPF_REG_0, size));
	else
		emit(dlp, BPF_ALU64_IMM(BPF_SUB, BPF_REG_0, size));

	emit(dlp,  BPF_BRANCH_REG(BPF_JLE, reg, BPF_REG_0, lbl_ok));

	dt_cg_probe_error(yypcb, DTRACEFLT_BADSIZE, isreg, size);

	emitl(dlp, lbl_ok,
		   BPF_NOP());
	dt_regset_free(drp, BPF_REG_0);
}

/*
 * Convert an access-checked alloca pointer value into an actual scratchmem
 * pointer.
 */
static void
dt_cg_alloca_ptr(dt_irlist_t *dlp, dt_regset_t *drp, int dreg, int sreg)
{
	if (dreg == sreg) {
		int	reg;

		if ((reg = dt_regset_alloc(drp)) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
		dt_cg_access_dctx(reg, dlp, drp, DCTX_SCRATCHMEM);
		emit(dlp,  BPF_ALU64_REG(BPF_ADD, sreg, reg));
		dt_regset_free(drp, reg);
	} else {
		dt_cg_access_dctx(dreg, dlp, drp, DCTX_SCRATCHMEM);
		emit(dlp,  BPF_ALU64_REG(BPF_ADD, dreg, sreg));
	}
}

/*
 * Convert an alloca pointer value into an actual scratchmem pointer.
 * Generate code to check a pointer argument (specifically for subroutine
 * arguments) to ensure that it is not NULL.  If the arguent pointer is an
 * alloca pointer, we also need to perform an access check and convert the
 * alloca pointer value into a real pointer.
 */
static void
dt_cg_check_ptr_arg(dt_irlist_t *dlp, dt_regset_t *drp, dt_node_t *dnp,
		    dt_node_t *size)
{
	if (dnp->dn_flags & DT_NF_ALLOCA) {
		if (size == NULL) {
			dtrace_diftype_t	vtype;

			dt_node_diftype(yypcb->pcb_hdl, dnp, &vtype);
			dt_cg_alloca_access_check(dlp, drp, dnp->dn_reg,
						  DT_ISIMM, vtype.dtdt_size);
		} else
			dt_cg_alloca_access_check(dlp, drp, dnp->dn_reg,
						  DT_ISREG, size->dn_reg);

		dt_cg_alloca_ptr(dlp, drp, dnp->dn_reg, dnp->dn_reg);
	} else
		dt_cg_check_notnull(dlp, drp, dnp->dn_reg);
}

static void dt_cg_setx(dt_irlist_t *dlp, int reg, uint64_t x);

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
	int			not_null = 0;
	int			cflags = pcb->pcb_stmt->dtsd_clauseflags;

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
		dt_cg_node(dnp, dlp, drp);
		dt_node_diftype(dtp, dnp, &vtype);
		size = vtype.dtdt_size;

		/*
		 * A DEREF of a REF node does not get resolved in dt_cg_node()
		 * because the ref node already holds the pointer.  But for
		 * alloca pointers, that will be the offset into scratchmem so
		 * we still need to turn it into a real pointer here.
		 */
		if (dnp->dn_kind == DT_NODE_OP1 &&
		    dnp->dn_op == DT_TOK_DEREF && (dnp->dn_flags & DT_NF_REF) &&
		    (dnp->dn_child->dn_flags & DT_NF_ALLOCA)) {
			dt_cg_alloca_access_check(dlp, drp, dnp->dn_reg,
						  DT_ISIMM, size);
			dt_cg_alloca_ptr(dlp, drp, dnp->dn_reg, dnp->dn_reg);
			not_null = 1;
		}
	}

	if (kind == DTRACEACT_USYM ||
	    kind == DTRACEACT_UMOD ||
	    kind == DTRACEACT_UADDR) {
		off = dt_rec_add(dtp, dt_cg_fill_gap, kind, 16, 8, NULL, arg);

		/* preface the value with the user process tgid */
		if (dt_regset_xalloc_args(drp) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
		dt_regset_xalloc(drp, BPF_REG_0);
		emit(dlp, BPF_CALL_HELPER(BPF_FUNC_get_current_pid_tgid));
		dt_regset_free_args(drp);
		emit(dlp, BPF_ALU64_IMM(BPF_AND, BPF_REG_0, 0xffffffff));
		emit(dlp, BPF_STORE(BPF_DW, BPF_REG_9, off, BPF_REG_0));
		dt_regset_free(drp, BPF_REG_0);

		/* then store the value */
		emit(dlp, BPF_STORE(BPF_DW, BPF_REG_9, off + 8, dnp->dn_reg));
		dt_regset_free(drp, dnp->dn_reg);

		goto ok;
	}

	if (dt_node_is_scalar(dnp) || dt_node_is_float(dnp) ||
	    dnp->dn_kind == DT_NODE_AGG) {
		off = dt_rec_add(dtp, dt_cg_fill_gap, kind, size, size, pfp,
				 arg);

		emit(dlp, BPF_STOREX(size, BPF_REG_9, off, dnp->dn_reg));
		dt_regset_free(drp, dnp->dn_reg);

		goto ok;
	} else if (dt_node_is_string(dnp)) {
		size_t	strsize = dtp->dt_options[DTRACEOPT_STRSIZE];

		if (!not_null)
			dt_cg_check_ptr_arg(dlp, drp, dnp, NULL);

		TRACE_REGSET("store_val(): Begin ");
		off = dt_rec_add(dtp, dt_cg_fill_gap, kind, size + 1,
				 1, pfp, arg);

		/*
		 * Copy the string data (no more than STRSIZE + 1 bytes) to the
		 * buffer at (%r9 + off).  We depend on the fact that
		 * probe_read_str() stops at the terminating NUL byte.
		 */
		if (dt_regset_xalloc_args(drp) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

		emit(dlp, BPF_MOV_REG(BPF_REG_1, BPF_REG_9));
		emit(dlp, BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, off));
		emit(dlp, BPF_MOV_IMM(BPF_REG_2, strsize + 1));
		emit(dlp, BPF_MOV_REG(BPF_REG_3, dnp->dn_reg));
		dt_regset_free(drp, dnp->dn_reg);
		dt_cg_tstring_free(pcb, dnp);
		dt_regset_xalloc(drp, BPF_REG_0);
		emit(dlp, BPF_CALL_HELPER(BPF_FUNC_probe_read_str));
		dt_regset_free_args(drp);
		dt_regset_free(drp, BPF_REG_0);
		TRACE_REGSET("store_val(): End   ");

		goto ok;
	}

	/* Handle tracing of by-ref values (arrays, struct, union). */
	if ((dnp->dn_flags & DT_NF_REF) || (arg & DT_NF_REF)) {
		off = dt_rec_add(dtp, dt_cg_fill_gap, kind, size, 2, pfp, arg);

		TRACE_REGSET("store_val(): Begin ");
		if (!not_null)
			dt_cg_check_notnull(dlp, drp, dnp->dn_reg);

		if (dt_regset_xalloc_args(drp) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

		emit(dlp, BPF_MOV_REG(BPF_REG_1, BPF_REG_9));
		emit(dlp, BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, off));
		emit(dlp, BPF_MOV_IMM(BPF_REG_2, size));
		emit(dlp, BPF_MOV_REG(BPF_REG_3, dnp->dn_reg));
		dt_regset_free(drp, dnp->dn_reg);
		dt_regset_xalloc(drp, BPF_REG_0);
		emit(dlp, BPF_CALL_HELPER(BPF_FUNC_probe_read));
		dt_regset_free_args(drp);

		dt_regset_free(drp, BPF_REG_0);
		TRACE_REGSET("store_val(): End   ");

		goto ok;
	}

	return -1;

ok:
	/*
	 * If this clause contains a speculate() statement, we need to
	 * generate code to verify that the amount of data recorded for this
	 * speculation (incl. what was just added) does not exceed the
	 * specsize limit.  If it does, further execution of this clause
	 * should be aborted.
	 */
	if (cflags & DT_CLSFLAG_SPECULATE) {
		dt_ident_t	*idp = dt_dlib_get_map(dtp, "state");
		uint_t		lbl_ok = dt_irlist_label(dlp);
		size_t		specsize = dtp->dt_options[DTRACEOPT_SPECSIZE];

		assert(idp);

		/*
		 * if (*((uint32_t *)&buf[DBUF_SPECID]) != 0) {
		 *     if (dctx->mst->specsize + off + size >
		 *	   dtp->dt_options[DTRACEOPT_SPECSIZE]) {
		 *	   state[DT_STATE_SPEC_DROPS]++;
		 *
		 *	   return;
		 *     }
		 * }
		 */
		if (dt_regset_xalloc_args(drp) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

		dt_regset_xalloc(drp, BPF_REG_0);
		emit(dlp,  BPF_LOAD(BPF_W, BPF_REG_0, BPF_REG_9, DBUF_SPECID));
		emit(dlp,  BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_0, 0, lbl_ok));

		dt_cg_access_dctx(BPF_REG_0, dlp, drp, DCTX_MST);
		emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_0, DMST_SPECSIZE));
		emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_0, off + size));
		dt_cg_setx(dlp, BPF_REG_1, specsize);
		emit(dlp,  BPF_BRANCH_REG(BPF_JLE, BPF_REG_0, BPF_REG_1, lbl_ok));

		dt_cg_xsetx(dlp, idp, DT_LBL_NONE, BPF_REG_1, idp->di_id);
		emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_2, BPF_REG_FP, DT_STK_SP));
		emit(dlp,  BPF_STORE_IMM(BPF_DW, BPF_REG_2, 0, DT_STATE_SPEC_DROPS));
		emit(dlp,  BPF_CALL_HELPER(BPF_FUNC_map_lookup_elem));
		emit(dlp,  BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_0, 0, pcb->pcb_exitlbl));
		emit(dlp,  BPF_MOV_IMM(BPF_REG_1, 1));
		emit(dlp,  BPF_XADD_REG(BPF_W, BPF_REG_0, 0, BPF_REG_1));
		emit(dlp,  BPF_JUMP(pcb->pcb_exitlbl));

		emitl(dlp, lbl_ok,
			   BPF_NOP());

		dt_regset_free_args(drp);
		dt_regset_free(drp, BPF_REG_0);
	}

	return 0;
}

static void
dt_cg_clsflags(dt_pcb_t *pcb, dtrace_actkind_t kind, const dt_node_t *dnp)
{
	int		*cfp = &pcb->pcb_stmt->dtsd_clauseflags;

	if (DTRACEACT_ISDESTRUCTIVE(kind))
		*cfp |= DT_CLSFLAG_DESTRUCT;

	if (kind == DTRACEACT_COMMIT) {
		if (*cfp & (DT_CLSFLAG_DATAREC | DT_CLSFLAG_AGGREGATION))
			dnerror(dnp, D_COMM_DREC, "commit( ) may "
			    "not follow data-recording action(s)\n");

		*cfp |= DT_CLSFLAG_COMMIT | DT_CLSFLAG_COMMIT_DISCARD;
		return;
	}

	if (kind == DTRACEACT_DISCARD) {
		*cfp |= DT_CLSFLAG_COMMIT_DISCARD;
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
		if (DTRACEACT_ISDESTRUCTIVE(kind))
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

	*cfp |= DT_CLSFLAG_DATAREC;
}

/*
 * Get offsetof(structname, membername) information from CTF.
 * Optionally, also get member size.
 */
int
dt_cg_ctf_offsetof(const char *structname, const char *membername,
		   size_t *sizep, int relaxed)
{
	dtrace_typeinfo_t sym;
	ctf_file_t *ctfp;
	ctf_membinfo_t ctm;

	if (dtrace_lookup_by_type(yypcb->pcb_hdl, DTRACE_OBJ_EVERY, structname,
				  &sym))
		longjmp(yypcb->pcb_jmpbuf, EDT_NOCTF);

	ctfp = sym.dtt_ctfp;
	if (!ctfp)		/* Should never happen. */
		longjmp(yypcb->pcb_jmpbuf, EDT_NOCTF);
	if (ctf_member_info(ctfp, sym.dtt_type, membername, &ctm) == CTF_ERR) {
		if (relaxed)
			return -1;

		longjmp(yypcb->pcb_jmpbuf, EDT_NOCTF);
	}

	if (sizep)
		*sizep = ctf_type_size(ctfp, ctm.ctm_type);

	return (ctm.ctm_offset / NBBY);
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

	dt_cg_store_val(pcb, anp, DTRACEACT_LIBACT, NULL, DT_ACT_CLEAR);
}

/*
 * Mark a speculation as committed/discarded and ready for draining.
 *
 * The speculation ID must be in IDREG (true for both commit and discard).
 *
 * If the speculation does not exist, nothing will be done: the consumer has to
 * detect this itself if the speculation is inactive (out-of-range values
 * fault and do not write a commit/discard record).
 */
static void
dt_cg_spec_set_drainable(dt_pcb_t *pcb, dt_node_t *dnp, int idreg)
{
	dt_regset_t	*drp = pcb->pcb_regs;
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	dt_ident_t	*idp;
	uint_t		lbl_ok = dt_irlist_label(dlp);

	idp = dt_dlib_get_func(yypcb->pcb_hdl, "dt_speculation_set_drainable");
	assert(idp != NULL);

	/*
	 *	spec = dt_speculation_set_drainable(ctx, id);
	 *				// call dt_speculation_commit_discard
	 *				//     (%r1 ... %r5 clobbered)
	 *	if (rc == 0)		// jeq %r0, 0, lbl_ok
	 *		goto ok;
	 *	return rc;		// mov %dn_reg, %r0
	 * ok:
	 */

	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	dt_cg_access_dctx(BPF_REG_1, dlp, drp, -1);
	emit(dlp,  BPF_MOV_REG(BPF_REG_2, idreg));
	dt_regset_xalloc(drp, BPF_REG_0);
	emite(dlp, BPF_CALL_FUNC(idp->di_id), idp);
	emit(dlp,  BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_0, 0, lbl_ok));
	emit(dlp,  BPF_RETURN());
	emitl(dlp, lbl_ok,
		   BPF_NOP());
	dt_regset_free(drp, BPF_REG_0);
	dt_regset_free_args(drp);
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
	dt_regset_t	*drp = pcb->pcb_regs;

	dt_cg_node(dnp->dn_args, &pcb->pcb_ir, drp);
	dt_cg_spec_set_drainable(pcb, dnp, dnp->dn_args->dn_reg);
	dt_regset_free(drp, dnp->dn_args->dn_reg);
	dt_cg_store_val(pcb, dnp->dn_args, DTRACEACT_COMMIT, NULL, DT_ACT_COMMIT);
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
	dt_regset_t	*drp = pcb->pcb_regs;

	dt_cg_node(dnp->dn_args, &pcb->pcb_ir, drp);
	dt_cg_spec_set_drainable(pcb, dnp, dnp->dn_args->dn_reg);
	dt_regset_free(drp, dnp->dn_args->dn_reg);
	dt_cg_store_val(pcb, dnp->dn_args, DTRACEACT_DISCARD, NULL, DT_ACT_DISCARD);
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
	dt_rec_add(pcb->pcb_hdl, dt_cg_fill_gap, DTRACEACT_LIBACT, 0, 1, NULL,
		   DT_ACT_FTRUNCATE);
}

static void
dt_cg_act_jstack(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind)
{
	dnerror(dnp, D_UNKNOWN, "jstack() is not implemented (yet)\n");
	/* FIXME: Needs implementation */
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
	dnerror(dnp, D_UNKNOWN, "panic() is not implemented (yet)\n");
	/* FIXME: Needs implementation */
}

static void
dt_cg_act_pcap(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind)
{
	dtrace_hdl_t	*dtp = pcb->pcb_hdl;
	dt_regset_t	*drp = pcb->pcb_regs;
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	dt_node_t	*addr = dnp->dn_args;
	dt_node_t	*proto = addr->dn_list;
	uint_t		time_off, size_off, data_off, off;
	uint_t		lbl_lenok = dt_irlist_label(dlp);
	uint64_t	pcapsz = dtp->dt_options[DTRACEOPT_PCAPSIZE];
	int		lenreg;

	TRACE_REGSET("pcap(): Begin ");

	/*
	 * Create records for timestamp, size, protocol id, and packet data.
	 * The protocol id is stored immediately from the passed in argument.
	 */
	time_off = dt_rec_add(dtp, dt_cg_fill_gap, DTRACEACT_PCAP,
			      sizeof(uint64_t), sizeof(uint64_t), NULL, 0);
	size_off = dt_rec_add(dtp, dt_cg_fill_gap, DTRACEACT_PCAP,
			      sizeof(uint32_t), sizeof(uint32_t), NULL, 0);
	dt_cg_store_val(pcb, proto, DTRACEACT_PCAP, NULL, 0);
	data_off = dt_rec_add(dtp, dt_cg_fill_gap, DTRACEACT_PCAP,
			      pcapsz, 1, NULL, 0);

	/* Store timestamp (ns since boot time). */
	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	dt_regset_xalloc(drp, BPF_REG_0);
	emit(dlp,  BPF_CALL_HELPER(BPF_FUNC_ktime_get_ns));
	dt_regset_free_args(drp);
	emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_9, time_off, BPF_REG_0));
	dt_regset_free(drp, BPF_REG_0);

	/*
	 * Determine and store the packet data size.
	 * The size of the data to be copied (and reported as size of the
	 * packet capture) is the lesser of the linear data size
	 * (skb->len - skb->data_len) and the capture size.
	 */
	dt_cg_node(addr, dlp, drp);

	off = dt_cg_ctf_offsetof("struct sk_buff", "len", NULL, 0);

	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	emit(dlp,  BPF_MOV_REG(BPF_REG_3, addr->dn_reg));
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_3, off));
	emit(dlp,  BPF_MOV_IMM(BPF_REG_2, sizeof(uint32_t)));
	emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_1, BPF_REG_FP, DT_STK_SP));
	dt_regset_xalloc(drp, BPF_REG_0);
	emit(dlp,  BPF_CALL_HELPER(dtp->dt_bpfhelper[BPF_FUNC_probe_read_kernel]));
	dt_regset_free_args(drp);
	dt_regset_free(drp, BPF_REG_0);

	if ((lenreg = dt_regset_alloc(drp)) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	emit(dlp,  BPF_LOAD(BPF_DW, lenreg, BPF_REG_FP, DT_STK_SP));
	emit(dlp,  BPF_LOAD(BPF_W, lenreg, lenreg, 0));

	off = dt_cg_ctf_offsetof("struct sk_buff", "data_len", NULL, 0);

	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	emit(dlp,  BPF_MOV_REG(BPF_REG_3, addr->dn_reg));
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_3, off));
	emit(dlp,  BPF_MOV_IMM(BPF_REG_2, sizeof(uint32_t)));
	emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_1, BPF_REG_FP, DT_STK_SP));
	dt_regset_xalloc(drp, BPF_REG_0);
	emit(dlp,  BPF_CALL_HELPER(dtp->dt_bpfhelper[BPF_FUNC_probe_read_kernel]));
	dt_regset_free_args(drp);
	emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_FP, DT_STK_SP));
	emit(dlp,  BPF_LOAD(BPF_W, BPF_REG_0, BPF_REG_0, 0));

	emit(dlp,  BPF_ALU64_REG(BPF_SUB, lenreg, BPF_REG_0));
	dt_regset_free(drp, BPF_REG_0);
	emit(dlp,  BPF_BRANCH_IMM(BPF_JLE, lenreg, pcapsz, lbl_lenok));
	emit(dlp,  BPF_MOV_IMM(lenreg, pcapsz));
	emitl(dlp, lbl_lenok,
		   BPF_STORE(BPF_W, BPF_REG_9, size_off, lenreg));

	/* Copy the packet data to the output buffer. */
	off = dt_cg_ctf_offsetof("struct sk_buff", "data", NULL, 0);

	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	emit(dlp,  BPF_MOV_REG(BPF_REG_3, addr->dn_reg));
	dt_regset_free(drp, addr->dn_reg);
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_3, off));
	emit(dlp,  BPF_MOV_IMM(BPF_REG_2, sizeof(uint64_t)));
	emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_1, BPF_REG_FP, DT_STK_SP));
	dt_regset_xalloc(drp, BPF_REG_0);
	emit(dlp,  BPF_CALL_HELPER(dtp->dt_bpfhelper[BPF_FUNC_probe_read_kernel]));
	dt_regset_free_args(drp);
	dt_regset_free(drp, BPF_REG_0);

	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	emit(dlp,  BPF_MOV_REG(BPF_REG_2, lenreg));
	dt_regset_free(drp, lenreg);
	emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_3, BPF_REG_FP, DT_STK_SP));
	emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_3, BPF_REG_3, 0));
	emit(dlp,  BPF_MOV_REG(BPF_REG_1, BPF_REG_9));
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, data_off));
	dt_regset_xalloc(drp, BPF_REG_0);
	emit(dlp,  BPF_CALL_HELPER(dtp->dt_bpfhelper[BPF_FUNC_probe_read_kernel]));
	dt_regset_free_args(drp);
	dt_regset_free(drp, BPF_REG_0);

	TRACE_REGSET("pcap(): End   ");
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
	dt_node_t	*opt = dnp->dn_args;
	dt_node_t	*val = opt->dn_list;

	TRACE_REGSET("setopt(): Begin ");
	dt_cg_store_val(pcb, opt, DTRACEACT_LIBACT, NULL, DT_ACT_SETOPT);

	/* If no value is given, we default to NULL. */
	if (val == NULL)
		val = dt_node_int(0);
	dt_cg_store_val(pcb, val, DTRACEACT_LIBACT, NULL, DT_ACT_SETOPT);
	TRACE_REGSET("setopt(): End   ");
}

/*
 * Signal that subsequent tracing output in the current clause should be kept
 * back pending a commit() or discard() for the speculation with the given id.
 *
 * Updates the specid in the output buffer header, rather than emitting a new
 * record into it.  The dctx->mst->specsize value is initialized with the size
 * of the data thus far recorded for this speculation.
 */
static void
dt_cg_act_speculate(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind)
{
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	dt_regset_t	*drp = pcb->pcb_regs;
	dt_ident_t	*idp;
	uint_t		lbl_exit = pcb->pcb_exitlbl;

	idp = dt_dlib_get_func(yypcb->pcb_hdl, "dt_speculation_speculate");
	assert(idp != NULL);

	dt_cg_node(dnp->dn_args, dlp, drp);
	dnp->dn_reg = dnp->dn_args->dn_reg;

	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	/*
	 *	dt_bpf_specs_t *spec = dt_speculation_speculate(specid);
	 *				// lddw %r1, %dn_reg
	 *				// call dt_speculation_speculate
	 *				//     (%r1 ... %r5 clobbered)
	 *				//     (%r0 = error return)
	 *	if (spec == 0)		// jne %r0, 0, lbl_exit
	 *		goto exit;
	 *	*((uint32_t *)&buf[DBUF_SPECID]) = specid;
	 *				// mov [%r9 + DBUF_SPECID], %dn_reg
	 *	dctx->mst->specsize = spec->size;
	 *	exit:			// nop
	 */

	emit(dlp, BPF_STORE(BPF_W, BPF_REG_FP, DT_STK_SPILL(0),
		dnp->dn_reg));
	emit(dlp, BPF_MOV_REG(BPF_REG_1, dnp->dn_reg));
	dt_regset_xalloc(drp, BPF_REG_0);
	emite(dlp, BPF_CALL_FUNC(idp->di_id), idp);
	dt_regset_free_args(drp);
	emit(dlp, BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_0, 0, lbl_exit));
	emit(dlp, BPF_STORE(BPF_W, BPF_REG_9, DBUF_SPECID, dnp->dn_reg));
	/* We need a register, so we re-use dnp->dn_reg. */
	emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_0, offsetof(dt_bpf_specs_t, size)));
	dt_cg_access_dctx(dnp->dn_reg, dlp, drp, DCTX_MST);
	emit(dlp,  BPF_STORE(BPF_DW, dnp->dn_reg, DMST_SPECSIZE, BPF_REG_0));
	dt_regset_free(drp, BPF_REG_0);
	dt_regset_free(drp, dnp->dn_reg);
}

static uint64_t
dt_cg_stack_arg(dtrace_hdl_t *dtp, dt_node_t *dnp, dtrace_actkind_t kind)
{
	int		nframes;
	int		strsize = 0;
	dt_node_t	*arg0 = dnp->dn_args;
	dt_node_t	*arg1 = arg0 != NULL ? arg0->dn_list : NULL;
	int		indopt, def, inderr;
	char		*fname;

	if (kind == DTRACEACT_USTACK) {
		indopt = DTRACEOPT_USTACKFRAMES;
		def = _dtrace_ustackframes;
		inderr = D_USTACK_FRAMES;
		fname = "ustack";
	} else if (kind == DTRACEACT_STACK) {
		indopt = DTRACEOPT_STACKFRAMES;
		def = _dtrace_stackframes;
		inderr = D_STACK_SIZE;
		fname = "stack";
	} else {
		assert(0);
	}

	/* Get the default number. */
	nframes = dtp->dt_options[indopt];
	if (nframes == DTRACEOPT_UNSET)
		nframes = def;

	/* Get the specified value, if any. */
	if (arg0 != NULL) {
		if (!dt_node_is_posconst(arg0))
			dnerror(arg0, inderr,
				"%s( ) argument #1 must be a non-zero positive integer constant\n",
				fname);
		nframes = arg0->dn_value;
	}

	/* If more frames are requested than allowed, silently reduce nframes. */
	if (nframes > dtp->dt_options[DTRACEOPT_MAXFRAMES])
		nframes = dtp->dt_options[DTRACEOPT_MAXFRAMES];

	/* For user stacks, process one more argument. */
	if (kind == DTRACEACT_USTACK && arg1 != NULL) {
		if (arg1->dn_kind != DT_NODE_INT ||
		    ((arg1->dn_flags & DT_NF_SIGNED) &&
		    (int64_t)arg1->dn_value < 0))
			dnerror(arg1, D_USTACK_STRSIZE,
				"ustack( ) argument #2 must be a positive integer constant\n");

		/* FIXME: for now, accept non-zero strsize, but it does nothing */
		strsize = arg1->dn_value;
	}

	return DTRACE_USTACK_ARG(nframes, strsize);
}

/*
 * Call the bpf_get_stack() helper function.
 *
 * dnp->dn_arg has optional, integer-constant sizing information.
 *
 * If reg>=0, reg is a register holding the output pointer.
 * If reg<0, we write the stack to the output buffer in %r9.
 *
 * These cases treat "off" differently:
 *   - reg>=0: "off" tracks how far the pointer in "reg" has
 *             been advanced, both at function entry and return
 *
 *   - reg<0: "off" is basically just a local variable since
 *            offsets with the %r9 buffer are managed by
 *            dt_rec_add()
 *
 * The "kind" argument is either DTRACEACT_STACK or DTRACEACT_USTACK.
 */
static int
dt_cg_act_stack_sub(dt_pcb_t *pcb, dt_node_t *dnp, int reg, int off, dtrace_actkind_t kind)
{
	dtrace_hdl_t	*dtp = pcb->pcb_hdl;
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	dt_regset_t	*drp = pcb->pcb_regs;
	uint64_t	arg;
	int		nframes, stacksize, prefsz, align = sizeof(uint64_t);
	uint_t		lbl_valid = dt_irlist_label(dlp);
	dt_ident_t	*skip = dt_dlib_get_var(dtp, "STACK_SKIP");

	assert(skip != NULL);

	/* Get sizing information from dnp->dn_arg. */
	arg = dt_cg_stack_arg(dtp, dnp, kind);
	prefsz = kind == DTRACEACT_USTACK ? sizeof(uint64_t) : 0;
	nframes = DTRACE_USTACK_NFRAMES(arg);
	stacksize = nframes * sizeof(uint64_t);

	/* Handle alignment and reserve space in the output buffer. */
	if (reg >= 0) {
		uint_t	nextoff;
		nextoff = (off + (align - 1)) & ~(align - 1);
		if (off < nextoff)
			emit(dlp,  BPF_ALU64_IMM(BPF_ADD, reg, nextoff - off));
		off = nextoff + prefsz + stacksize;
	} else {
		off = dt_rec_add(dtp, dt_cg_fill_gap, kind,
				 prefsz + stacksize, align, NULL, arg);
	}

	/* Write the tgid. */
	if (kind == DTRACEACT_USTACK) {
		if (dt_regset_xalloc_args(drp) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
		dt_regset_xalloc(drp, BPF_REG_0);
		emit(dlp,  BPF_CALL_HELPER(BPF_FUNC_get_current_pid_tgid));
		dt_regset_free_args(drp);
		emit(dlp,  BPF_ALU64_IMM(BPF_AND, BPF_REG_0, 0xffffffff));
		if (reg >= 0)
			emit(dlp,  BPF_STORE(BPF_DW, reg, 0, BPF_REG_0));
		else
			emit(dlp,  BPF_STORE(BPF_DW, BPF_REG_9, off, BPF_REG_0));
		dt_regset_free(drp, BPF_REG_0);
	}

	/* Call bpf_get_stack(ctx, buf, size, flags). */
	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	dt_cg_access_dctx(BPF_REG_1, dlp, drp, DCTX_CTX);
	if (reg >= 0) {
		emit(dlp,  BPF_MOV_REG(BPF_REG_2, reg));
		if (prefsz)
			emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, prefsz));
	} else {
		emit(dlp,  BPF_MOV_REG(BPF_REG_2, BPF_REG_9));
		emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, off + prefsz));
	}
	emit(dlp,  BPF_MOV_IMM(BPF_REG_3, stacksize));
	if (kind == DTRACEACT_USTACK)
		emit(dlp,  BPF_MOV_IMM(BPF_REG_4, BPF_F_USER_STACK));
	else {
		emite(dlp, BPF_MOV_IMM(BPF_REG_4, -1), skip);
		emit(dlp,  BPF_ALU64_IMM(BPF_AND, BPF_REG_4, BPF_F_SKIP_FIELD_MASK));
	}
	dt_regset_xalloc(drp, BPF_REG_0);
	emit(dlp,  BPF_CALL_HELPER(BPF_FUNC_get_stack));
	dt_regset_free_args(drp);
	emit(dlp,  BPF_BRANCH_IMM(BPF_JSGE, BPF_REG_0, 0, lbl_valid));
	dt_regset_free(drp, BPF_REG_0);
	dt_cg_probe_error(pcb, DTRACEFLT_BADSTACK, DT_ISIMM, 0);
	emitl(dlp, lbl_valid,
		   BPF_NOP());

	/* Finish. */
	if (reg >= 0)
		emit(dlp,  BPF_ALU64_IMM(BPF_ADD, reg, prefsz + stacksize));

	return off;
}

static void
dt_cg_act_stack(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind)
{
	dt_cg_act_stack_sub(pcb, dnp, -1, 0, kind);
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
	int		flags = 0;

	if (dt_node_is_void(arg))
		dnerror(arg, D_TRACE_VOID,
			"trace( ) may not be applied to a void expression\n");

	if (dt_node_is_dynamic(arg))
		dnerror(arg, D_TRACE_DYN,
			"trace( ) may not be applied to a dynamic "
			"expression\n");

	if (arg->dn_flags & DT_NF_REF)
		flags = DT_NF_REF;
	else if (arg->dn_flags & DT_NF_SIGNED)
		flags = DT_NF_SIGNED;

	if (dt_cg_store_val(pcb, arg, DTRACEACT_DIFEXPR, NULL, flags) == -1)
		dnerror(arg, D_PROTO_ARG,
			"trace( ) argument #1 is incompatible with prototype:\n"
			"\tprototype: scalar or string\n\t argument: %s\n",
			dt_node_type_name(arg, n, sizeof(n)));
}

static void
dt_cg_act_tracemem(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind)
{
	char		n[DT_TYPE_NAMELEN];
	dt_node_t	*addr = dnp->dn_args;
	dt_node_t	*nsiz = addr->dn_list;
	dt_node_t	*dsiz = nsiz->dn_list;
	dtrace_hdl_t	*dtp = pcb->pcb_hdl;
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	dt_regset_t	*drp = pcb->pcb_regs;
	uint_t		off;

	if (dt_node_is_integer(addr) == 0 && dt_node_is_pointer(addr) == 0)
		dnerror(addr, D_TRACEMEM_ADDR,
		    "tracemem( ) argument #1 is incompatible with prototype:\n"
		    "\tprototype: pointer or integer\n"
		    "\t argument: %s\n", dt_node_type_name(addr, n, sizeof(n)));

	if (dt_node_is_posconst(nsiz) == 0)
		dnerror(nsiz, D_TRACEMEM_SIZE,
		    "tracemem( ) argument #2 must be a non-zero positive integral constant expression\n");

	dt_cg_node(addr, dlp, drp);
	dt_cg_node(nsiz, dlp, drp);

	off = dt_rec_add(dtp, dt_cg_fill_gap, DTRACEACT_TRACEMEM,
			 nsiz->dn_value, 1, NULL,
			 dsiz ? DTRACE_TRACEMEM_DYNAMIC
			      : DTRACE_TRACEMEM_STATIC);

	if (dsiz != NULL) {
		uint_t	Lokay = dt_irlist_label(dlp);

		/* The dt_cg_store_val() call evaluates dsiz. */
		dt_cg_store_val(pcb, dsiz, DTRACEACT_TRACEMEM, NULL,
				dsiz->dn_flags & DT_NF_SIGNED
					? DTRACE_TRACEMEM_SSIZE
					: DTRACE_TRACEMEM_SIZE);

		/*
		 * This is quite naughty.  We know dt_cg_store_val() just
		 * free'd dsiz->dn_reg and no register allocation took place
		 * after that, so we know we can re-alloc that register.
		 */
		dt_regset_xalloc(drp, dsiz->dn_reg);

		/*
		 * Decide whether to reduce the size of the data that is
		 * actually being recorded.  We do this only if the dynamic
		 * size is non-negative and less than the nsiz.  Note that the
		 * dsiz->dn_reg value has been sign-extended to 64 bits and we
		 * will never have an nsiz that is 1<<63 or greater.  So, just
		 * treat dsiz->dn_reg as an unsigned value.
		 */
		emit(dlp,  BPF_BRANCH_REG(BPF_JLE, nsiz->dn_reg, dsiz->dn_reg, Lokay));
		emit(dlp,  BPF_MOV_REG(nsiz->dn_reg, dsiz->dn_reg));
		emitl(dlp, Lokay,
			   BPF_NOP());

		dt_regset_free(drp, dsiz->dn_reg);
	}

	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	emit(dlp, BPF_MOV_REG(BPF_REG_1, BPF_REG_9));
	emit(dlp, BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, off));
	emit(dlp, BPF_MOV_REG(BPF_REG_2, nsiz->dn_reg));
	dt_regset_free(drp, nsiz->dn_reg);
	emit(dlp, BPF_MOV_REG(BPF_REG_3, addr->dn_reg));
	dt_regset_free(drp, addr->dn_reg);

	dt_regset_xalloc(drp, BPF_REG_0);
	emit(dlp, BPF_CALL_HELPER(BPF_FUNC_probe_read));
	dt_regset_free(drp, BPF_REG_0);
	dt_regset_free_args(drp);
}

static void
dt_cg_act_trunc(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind)
{
	dt_node_t	*anp, *trunc;
	dt_ident_t	*aid;
	char		n[DT_TYPE_NAMELEN];

	anp = dnp->dn_args;
	assert(anp != NULL);
	if (anp->dn_kind != DT_NODE_AGG)
		dnerror(dnp, D_TRUNC_AGGARG,
			"%s( ) argument #1 is incompatible with prototype:\n"
			"\tprototype: aggregation\n\t argument: %s\n",
			dnp->dn_ident->di_name,
			dt_node_type_name(anp, n, sizeof(n)));

	aid = anp->dn_ident;
	if (aid->di_gen == pcb->pcb_hdl->dt_gen &&
	    !(aid->di_flags & DT_IDFLG_MOD))
		dnerror(dnp, D_TRUNC_AGGBAD,
			"undefined aggregation: @%s\n", aid->di_name);

	trunc = anp->dn_list;
	if (trunc == NULL)
		trunc = dt_node_int(0);

	dt_cg_store_val(pcb, anp, DTRACEACT_LIBACT, NULL, DT_ACT_TRUNC);
	dt_cg_store_val(pcb, trunc, DTRACEACT_LIBACT, NULL, DT_ACT_TRUNC);
}

static void
dt_cg_act_print(dt_pcb_t *pcb, dt_node_t *dnp, dtrace_actkind_t kind)
{
	uint_t		addr_off, data_off;
	dt_node_t	*addr = dnp->dn_args;
	dt_irlist_t	*dlp = &pcb->pcb_ir;
	dt_regset_t	*drp = pcb->pcb_regs;
	dtrace_hdl_t	*dtp = pcb->pcb_hdl;
	ctf_file_t	*fp = addr->dn_ctfp;
	ctf_id_t	type = addr->dn_type;
	char		n[DT_TYPE_NAMELEN];
	dt_module_t	*dmp;
	dt_pfargv_t	*pfp;
	size_t		size;

	dmp = dt_module_lookup_by_ctf(dtp, fp);
	type = ctf_type_reference(fp, type);
	if (type == CTF_ERR)
		longjmp(yypcb->pcb_jmpbuf, EDT_CTF);
	size = ctf_type_size(fp, type);
	if (size == 0)
		dnerror(addr, D_PRINT_SIZE,
			"print( ) argument #1 reference has type '%s' with size 0; cannot print( ) it.\n",
			ctf_type_name(fp, type, n, sizeof(n)));

	pfp = dt_printf_create(dtp, dmp->dm_name);

	/* reserve space for addr/type, data/size */
	addr_off = dt_rec_add(dtp, dt_cg_fill_gap, DTRACEACT_PRINT,
			      sizeof(uint64_t), 8, pfp, type);
	data_off = dt_rec_add(dtp, dt_cg_fill_gap, DTRACEACT_PRINT,
			      size, 8, NULL, 0);

	dt_cg_node(addr, &pcb->pcb_ir, drp);
	dt_cg_check_ptr_arg(dlp, drp, addr, NULL);

	/* store address for later print()ing */
	emit(dlp, BPF_STORE(BPF_DW, BPF_REG_9, addr_off, addr->dn_reg));

	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	emit(dlp, BPF_MOV_REG(BPF_REG_3, addr->dn_reg));
	dt_regset_free(drp, addr->dn_reg);
	emit(dlp, BPF_MOV_REG(BPF_REG_1, BPF_REG_9));
	emit(dlp, BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, data_off));
	emit(dlp, BPF_MOV_IMM(BPF_REG_2, size));
	dt_regset_xalloc(drp, BPF_REG_0);
	emit(dlp, BPF_CALL_HELPER(BPF_FUNC_probe_read));
	dt_regset_free_args(drp);
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
	[DT_ACT_IDX(DT_ACT_STACK)]		= { &dt_cg_act_stack,
						    DTRACEACT_STACK },
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
	[DT_ACT_IDX(DT_ACT_USTACK)]		= { &dt_cg_act_stack,
						    DTRACEACT_USTACK },
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
	[DT_ACT_IDX(DT_ACT_PRINT)]		= { &dt_cg_act_print, },
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

static void
dt_cg_push_stack(int reg, dt_irlist_t *dlp, dt_regset_t *drp)
{
	int	treg;

	if ((treg = dt_regset_alloc(drp)) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	emit(dlp, BPF_LOAD(BPF_DW, treg, BPF_REG_FP, DT_STK_SP));
	emit(dlp, BPF_STORE(BPF_DW, treg, 0, reg));
	emit(dlp, BPF_ALU64_IMM(BPF_ADD, treg, -DT_STK_SLOT_SZ));
	emit(dlp, BPF_STORE(BPF_DW, BPF_REG_FP, DT_STK_SP, treg));
	dt_regset_free(drp, treg);
}

static void
dt_cg_pop_stack(int reg, dt_irlist_t *dlp, dt_regset_t *drp)
{
	int	treg;

	if ((treg = dt_regset_alloc(drp)) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	emit(dlp, BPF_LOAD(BPF_DW, treg, BPF_REG_FP, DT_STK_SP));
	emit(dlp, BPF_ALU64_IMM(BPF_ADD, treg, DT_STK_SLOT_SZ));
	emit(dlp, BPF_LOAD(BPF_DW, reg, treg, 0));
	emit(dlp, BPF_STORE(BPF_DW, BPF_REG_FP, DT_STK_SP, treg));
	dt_regset_free(drp, treg);
}

/*
 * Store a pointer to the 'memory block of zeros' in reg.
 */
static void
dt_cg_zerosptr(int reg, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dtrace_hdl_t	*dtp = yypcb->pcb_hdl;
	dt_ident_t	*zero_off = dt_dlib_get_var(dtp, "ZERO_OFF");

	dt_cg_access_dctx(reg, dlp, drp, DCTX_STRTAB);
	emite(dlp, BPF_ALU64_IMM(BPF_ADD, reg, -1), zero_off);
}

/*
 * Generate code to promote signed scalars (size < 64 bits) to native register
 * size (64 bits).
 */
static void
dt_cg_promote(const dt_node_t *dnp, ssize_t size, dt_irlist_t *dlp,
	      dt_regset_t *drp)
{
	if (dnp->dn_flags & DT_NF_SIGNED && size < sizeof(uint64_t)) {
		int	n = (sizeof(uint64_t) - size) * NBBY;

		emit(dlp, BPF_ALU64_IMM(BPF_LSH, dnp->dn_reg, n));
		emit(dlp, BPF_ALU64_IMM(BPF_ARSH, dnp->dn_reg, n));
	}
}

/*
 * Load a scalar from an arbitrary address.
 */
#define BPF_SZ_NONE	UINT_MAX

static void
dt_cg_load_scalar(dt_node_t *dnp, uint_t op, ssize_t size, dt_irlist_t *dlp,
		  dt_regset_t *drp)
{
	uint_t Lokay = dt_irlist_label(dlp);

	if (op == BPF_SZ_NONE)
		op = bpf_ldst_size(size, 0);

	/* Copy the scalar onto the stack. */
	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	emit(dlp, BPF_MOV_REG(BPF_REG_3, dnp->dn_reg));
	emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_1, BPF_REG_FP, DT_STK_SP));
	emit(dlp, BPF_MOV_IMM(BPF_REG_2, size));
	dt_regset_xalloc(drp, BPF_REG_0);
	emit(dlp, BPF_CALL_HELPER(BPF_FUNC_probe_read));
	dt_regset_free_args(drp);

	/* Report a fault if the copy failed. */
	emit(dlp,  BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_0, 0, Lokay));
	dt_regset_free(drp, BPF_REG_0);
	dt_cg_probe_error(yypcb, DTRACEFLT_BADADDR, DT_ISREG, dnp->dn_reg);
	emitl(dlp, Lokay,
		   BPF_NOP());

	/* Load the scalar from the stack. */
	emit(dlp, BPF_LOAD(BPF_DW, dnp->dn_reg, BPF_REG_FP, DT_STK_SP));
	emit(dlp, BPF_LOAD(op, dnp->dn_reg, dnp->dn_reg, 0));
	dt_cg_promote(dnp, size, dlp, drp);
}

static void
dt_cg_arglist(dt_ident_t *idp, dt_node_t *args, dt_irlist_t *dlp,
	      dt_regset_t *drp);

static void
dt_cg_prep_dvar(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp,
		dt_ident_t *idp, int isstore)
{
	uint_t		varid = idp->di_id - DIF_VAR_OTHER_UBASE;
	const char	*fn;
	dt_ident_t	*fnp;

	if (idp->di_kind == DT_IDENT_ARRAY) {
		/* Associative (global or TLS) array.  Cannot be in alloca space. */
		dt_node_t *args = isstore && dnp->dn_kind != DT_NODE_VAR
					? dnp->dn_left : dnp;

		fn = "dt_get_assoc";

		dt_cg_arglist(idp, args->dn_args, dlp, drp);

		if (dt_regset_xalloc_args(drp) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

		emit(dlp,  BPF_MOV_IMM(BPF_REG_1, varid));
		emit(dlp,  BPF_MOV_REG(BPF_REG_2, args->dn_args->dn_reg));
		dt_regset_free(drp, args->dn_args->dn_reg);
		emit(dlp,  BPF_MOV_IMM(BPF_REG_3, isstore));
		if (isstore) {
			dt_node_t	*rp = dnp->dn_right;

			/*
			 * Use an immediate move if we are assigning a literal
			 * 0 because future callers may not ensure dnp->dn_reg
			 * is actually allocated.
			 */
			if (rp->dn_kind == DT_NODE_INT && rp->dn_value == 0)
				emit(dlp,  BPF_MOV_IMM(BPF_REG_4, 0));
			else
				emit(dlp,  BPF_MOV_REG(BPF_REG_4, dnp->dn_reg));
		} else
			emit(dlp,  BPF_MOV_IMM(BPF_REG_4, 0));
		dt_cg_zerosptr(BPF_REG_5, dlp, drp);
	} else if (idp->di_flags & DT_IDFLG_TLS) {
		/* thread-local variables */
		fn = "dt_get_tvar";

		if (dt_regset_xalloc_args(drp) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

		emit(dlp,  BPF_MOV_IMM(BPF_REG_1, varid));
		emit(dlp,  BPF_MOV_IMM(BPF_REG_2, isstore));
		if (isstore) {
			/*
			 * Three cases:
			 *  - if called for a DT_NODE_VAR node, there will not
			 *    be a right child node and the caller will perform
			 *    the store explicitly.  Set nval to 1 (a non-zero
			 *    value) to indicate that this is not a delete
			 *    operation.
			 *  - if a literal 0 is in the right child node,
			 *    dnp->dn_reg may not be allocated, so we use an
			 *    immediate move
			 *  - otherwise we perform the typical store-0-deletes
			 *    handling
			 */
			if (dnp->dn_kind == DT_NODE_VAR)
				emit(dlp,  BPF_MOV_IMM(BPF_REG_3, 1));
			else {
				dt_node_t	*rp = dnp->dn_right;

				if (rp->dn_kind == DT_NODE_INT && rp->dn_value == 0)
					emit(dlp,  BPF_MOV_IMM(BPF_REG_3, 0));
				else
					emit(dlp,  BPF_MOV_REG(BPF_REG_3, dnp->dn_reg));
			}
		} else
			emit(dlp,  BPF_MOV_IMM(BPF_REG_3, 0));
		dt_cg_zerosptr(BPF_REG_4, dlp, drp);
	} else
		assert(0);

	fnp = dt_dlib_get_func(yypcb->pcb_hdl, fn);
	assert(fnp);

	dt_regset_xalloc(drp, BPF_REG_0);
	emite(dlp, BPF_CALL_FUNC(fnp->di_id), fnp);
	dt_regset_free_args(drp);
}

static void
dt_cg_load_var(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_ident_t	*idp = dt_ident_resolve(dnp->dn_ident);
	dt_ident_t	*fnp;

	idp->di_flags |= DT_IDFLG_DIFR;

	/* First handle dvars. */
	if (idp->di_kind == DT_IDENT_ARRAY || idp->di_flags & DT_IDFLG_TLS) {
		dt_cg_prep_dvar(dnp, dlp, drp, idp, 0);

		if ((dnp->dn_reg = dt_regset_alloc(drp)) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

		if (dnp->dn_flags & DT_NF_REF) {
			/*
			 * The BPF verifier knows we have a pointer that is
			 * map_value_or_null.  We will check later for null.
			 * If we first add an offset, however, the verifier
			 * objects.
			 *
			 * Check the value.  If 0, assign 0.  This allows the
			 * verifier to branch and handle map_value_or_null as
			 * two separate cases.
			 */
			uint_t	lbl_notnull = dt_irlist_label(dlp);
			uint_t	lbl_done = dt_irlist_label(dlp);

			emit(dlp,  BPF_BRANCH_IMM(BPF_JNE, BPF_REG_0, 0, lbl_notnull));
			emit(dlp,  BPF_MOV_IMM(dnp->dn_reg, 0));
			emit(dlp,  BPF_JUMP(lbl_done));
			emitl(dlp, lbl_notnull,
				   BPF_MOV_REG(dnp->dn_reg, BPF_REG_0));
			emitl(dlp, lbl_done,
				   BPF_NOP());
		} else {
			size_t	size = dt_node_type_size(dnp);
			uint_t	lbl_notnull = dt_irlist_label(dlp);
			uint_t	lbl_done = dt_irlist_label(dlp);

			emit(dlp,  BPF_BRANCH_IMM(BPF_JNE, BPF_REG_0, 0, lbl_notnull));
			emit(dlp,  BPF_MOV_IMM(dnp->dn_reg, 0));
			emit(dlp,  BPF_JUMP(lbl_done));
			emitl(dlp, lbl_notnull,
				   BPF_LOADX(size, dnp->dn_reg, BPF_REG_0, 0));
			dt_cg_promote(dnp, size, dlp, drp);

			emitl(dlp, lbl_done,
				   BPF_NOP());
		}

		dt_regset_free(drp, BPF_REG_0);

		return;
	}

	/* global and local variables */
	if ((idp->di_flags & DT_IDFLG_LOCAL) ||
	    (idp->di_id >= DIF_VAR_OTHER_UBASE)) {
		if ((dnp->dn_reg = dt_regset_alloc(drp)) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

		/* get pointer to BPF map */
		if (idp->di_flags & DT_IDFLG_LOCAL)
			dt_cg_access_dctx(dnp->dn_reg, dlp, drp, DCTX_LVARS);
		else
			dt_cg_access_dctx(dnp->dn_reg, dlp, drp, DCTX_GVARS);

		/* load the variable value or address */
		if (dt_node_is_string(dnp)) {
			/*
			 * Strings are a special case of by-reference.  If we have
			 * a NULL string, we want to set the pointer to 0.
			 */
			size_t	size = sizeof(DT_NULL_STRING);
			int	reg;
			uint_t	L1 = dt_irlist_label(dlp);
			uint_t	L2 = dt_irlist_label(dlp);

			assert(dnp->dn_flags & DT_NF_REF);

			if ((reg = dt_regset_alloc(drp)) == -1)
				longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
			emit(dlp,  BPF_LOADX(size, reg, dnp->dn_reg, idp->di_offset));
			emit(dlp,  BPF_BRANCH_IMM(BPF_JNE, reg, DT_NULL_STRING, L1));
			emit(dlp,  BPF_MOV_IMM(dnp->dn_reg, 0));
			emit(dlp,  BPF_JUMP(L2));
			emitl(dlp, L1,
				   BPF_ALU64_IMM(BPF_ADD, dnp->dn_reg, idp->di_offset));
			emitl(dlp, L2,
				   BPF_NOP());
			dt_regset_free(drp, reg);
		} else if (dnp->dn_flags & DT_NF_REF) {
			assert(!(dnp->dn_flags & DT_NF_ALLOCA));
			emit(dlp, BPF_ALU64_IMM(BPF_ADD, dnp->dn_reg, idp->di_offset));
		} else {
			size_t	size = dt_node_type_size(dnp);

			emit(dlp, BPF_LOADX(size, dnp->dn_reg, dnp->dn_reg, idp->di_offset));
			dt_cg_promote(dnp, size, dlp, drp);
		}

		return;
	}

	/* built-in variables */
	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	dt_cg_access_dctx(BPF_REG_1, dlp, drp, -1);
	emit(dlp, BPF_MOV_IMM(BPF_REG_2, idp->di_id));
	emit(dlp, BPF_MOV_IMM(BPF_REG_3, 0));
	fnp = dt_dlib_get_func(yypcb->pcb_hdl, "dt_get_bvar");
	assert(fnp != NULL);
	dt_regset_xalloc(drp, BPF_REG_0);
	emite(dlp, BPF_CALL_FUNC(fnp->di_id), fnp);
	dt_regset_free_args(drp);

	dt_cg_check_fault(yypcb);

	if ((dnp->dn_reg = dt_regset_alloc(drp)) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	emit(dlp,  BPF_MOV_REG(dnp->dn_reg, BPF_REG_0));
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
 * to generate the load instruction and bit shifts that extracts the value.
 */
static void
dt_cg_field_get(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp,
		ctf_file_t *fp, const ctf_membinfo_t *mp)
{
	ctf_encoding_t	e;
	uint64_t	shift;
	ssize_t		size;
	size_t		offset;
	uint_t		op;
	int		reg = dnp->dn_left->dn_reg;

	if (ctf_type_encoding(fp, mp->ctm_type, &e) != 0 || e.cte_bits > 64) {
		xyerror(D_UNKNOWN, "cg: bad field: member off %lu type <%ld> "
		    "oofset %u bits %u\n", mp->ctm_offset, mp->ctm_type,
		    e.cte_offset, e.cte_bits);
	}

	assert(dnp->dn_op == DT_TOK_PTR || dnp->dn_op == DT_TOK_DOT);

	offset = mp->ctm_offset + e.cte_offset;

	/* Advance to the byte where the bitfield starts (if needed). */
	if (offset >= NBBY) {
		emit(dlp, BPF_ALU64_IMM(BPF_ADD, reg, offset / NBBY));
		offset %= NBBY;
	}

	/* If this is a REF, we are done. */
	if (dnp->dn_flags & DT_NF_REF)
		return;

	op = dt_cg_ldsize(dnp, fp, mp->ctm_type, &size);
	if (dnp->dn_left->dn_flags & (DT_NF_ALLOCA | DT_NF_DPTR))
		emit(dlp, BPF_LOAD(op, reg, reg, 0));
	else
		dt_cg_load_scalar(dnp->dn_left, op, size, dlp, drp);

#if 0
	/*
	 * On little-endian architectures, offset counts from the right so
	 * offset itself is the amount we want to shift right to move the value
	 * bits to the little end of the register to mask them.
	 * On big-endian architectures, offset counts from the left so we must
	 * subtract (offset + cte_bits) from the size in bits we used for the
	 * load.  The size of our load in turn is found by rounding cte_bits up
	 * to a byte boundary and then finding the nearest power of two to this
	 * value (see clp2(), above).
	 *
	 * For signed values, we first shift left to ensure that the leftmost
	 * bit set the sign, so that a subsequent sign-extending right shift
	 * ensure we get the correct signed value.
	 */
	if (dnp->dn_flags & DT_NF_SIGNED) {
		/*
		 * reg <<= 64 - shift
		 * reg >>= 64 - bits
		 */
#ifdef _BIG_ENDIAN
		shift = clp2(P2ROUNDUP(e.cte_bits, NBBY) / NBBY) * NBBY -
			offset;
#else
		shift = offset + e.cte_bits;
#endif
		if (shift < 64)
			emit(dlp, BPF_ALU64_IMM(BPF_LSH, reg, 64 - shift));
		emit(dlp, BPF_ALU64_IMM(BPF_ARSH, reg, 64 - e.cte_bits));
	} else {
		/*
		 * reg >>= shift
		 * reg &= (1 << bits) - 1
		 */
#ifdef _BIG_ENDIAN
		shift = clp2(P2ROUNDUP(e.cte_bits, NBBY) / NBBY) * NBBY -
			(offset + e.cte_bits);
#else
		shift = offset;
#endif
		if (shift)
			emit(dlp, BPF_ALU64_IMM(BPF_RSH, reg, shift));
		emit(dlp, BPF_ALU64_IMM(BPF_AND, reg, (1ULL << e.cte_bits) - 1));
	}
#else
	/*
	 * Shift the loaded value left (64 - cte_bits - shift) bits to mask off
	 * higher order bits, and then shift right (64 - cte_bits) bits (with
	 * possible sign extending) to move the bitfield value to the low end
	 * of the register.
	 *
	 * On little-endian architectures, offset counts from the right so the
	 * shift we want is the offset itself.
	 *
	 * On big-endian architectures, offset counts from the left so we must
	 * subtract (offset + cte_bits) from the load size (in bits) to
	 * calculate the left shift amount.  The size of the load is found by
	 * rounding cte_bits up to a byte boundary and then determining the
	 * nearest power of two to this value (see clp2(), above).
	 */
#ifdef _BIG_ENDIAN
	shift = clp2(P2ROUNDUP(e.cte_bits, NBBY) / NBBY) * NBBY -
		(offset + e.cte_bits);
#else
	shift = offset;
#endif

	/*
	 * reg <<= 64 - bits - shift
	 * reg >>= 64 - bits			(sign-extending if signed)
	 */
	emit(dlp, BPF_ALU64_IMM(BPF_LSH, reg, 64 - e.cte_bits - shift));
	if (dnp->dn_flags & DT_NF_SIGNED)
		emit(dlp, BPF_ALU64_IMM(BPF_ARSH, reg, 64 - e.cte_bits));
	else
		emit(dlp, BPF_ALU64_IMM(BPF_RSH, reg, 64 - e.cte_bits));
#endif
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
	size_t offset;

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
		xyerror(D_UNKNOWN, "cg: bad field: member off %lu type <%ld> "
		    "offset %u bits %u\n", m.ctm_offset, m.ctm_type,
		    e.cte_offset, e.cte_bits);
	}

	offset = m.ctm_offset + e.cte_offset;

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
		(offset % NBBY + e.cte_bits);
#else
	shift = offset % NBBY;
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
	emit(dlp, BPF_LOAD(dt_cg_ldsize(dst, fp, m.ctm_type, NULL), r1, dst->dn_reg, 0));
	dt_cg_setx(dlp, r2, cmask);
	emit(dlp, BPF_ALU64_REG(BPF_AND, r1, r2));
	dt_cg_setx(dlp, r2, fmask);
	emit(dlp, BPF_ALU64_REG(BPF_AND, r2, src->dn_reg));
	if (shift > 0)
		emit(dlp, BPF_ALU64_IMM(BPF_LSH, r2, shift));
	emit(dlp, BPF_ALU64_REG(BPF_OR, r1, r2));
	dt_regset_free(drp, r2);

	return r1;
}

static void
dt_cg_store(dt_node_t *src, dt_irlist_t *dlp, dt_regset_t *drp, dt_node_t *dst)
{
	ctf_encoding_t	e;
	size_t		size;
	int		dreg = dst->dn_reg;

	/*
	 * If we're storing into a bit-field, the size of our store is found by
	 * rounding dst's cte_bits up to a byte boundary and then finding the
	 * nearest power of two to this value (see clp2(), above).
	 */
	if ((dst->dn_flags & DT_NF_BITFIELD) &&
	    ctf_type_encoding(dst->dn_ctfp, dst->dn_type, &e) != CTF_ERR)
		size = clp2(P2ROUNDUP(e.cte_bits, NBBY) / NBBY);
	else
		size = dt_node_type_size(dst);

	/*
	 * If we're storing into a writable lvalue that is a dereference of an
	 * alloca pointer or if we are storing into a writable alloca lvalue,
	 * we need to do bounds-checking and turn the offset value into a real
	 * pointer.
	 */
	if (dst->dn_flags & DT_NF_WRITABLE && dst->dn_flags & DT_NF_LVALUE &&
	    ((dst->dn_op == DT_TOK_DEREF &&
	      dst->dn_child->dn_flags & DT_NF_ALLOCA) ||
	     dst->dn_flags & DT_NF_ALLOCA)) {
		assert(!(dst->dn_flags & DT_NF_BITFIELD));

		if ((dreg = dt_regset_alloc(drp)) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

		dt_cg_alloca_access_check(dlp, drp, dst->dn_reg,
					  DT_ISIMM, size);
		dt_cg_alloca_ptr(dlp, drp, dreg, dst->dn_reg);
	}

	if (src->dn_flags & DT_NF_REF)
		dt_cg_memcpy(dlp, drp, dreg, src->dn_reg, size);
	else {
		int	sreg;

		if (dst->dn_flags & DT_NF_BITFIELD)
			sreg = dt_cg_field_set(src, dlp, drp, dst);
		else
			sreg = src->dn_reg;

		emit(dlp, BPF_STOREX(size, dreg, 0, sreg));

		if (sreg != src->dn_reg)
			dt_regset_free(drp, sreg);
	}

	if (dreg != dst->dn_reg)
		dt_regset_free(drp, dreg);
}

/*
 * Generate code for a typecast or for argument promotion from the type of the
 * actual to the type of the formal.  We need to generate code for casts when
 * a scalar type is being narrowed or changing signed-ness.  We first shift the
 * desired bits high (losing excess bits if narrowing) and then shift them down
 * using logical shift (unsigned result) or arithmetic shift (signed result).
 *
 * We also need to scalarize pointers if we are casting them to an integral type.
 */
static void
dt_cg_typecast(const dt_node_t *src, const dt_node_t *dst,
    dt_irlist_t *dlp, dt_regset_t *drp)
{
	/* If the destination type is '@' (any type) we need not cast. */
	if (dst->dn_ctfp == NULL && dst->dn_type == CTF_ERR)
		return;

	if (!dt_node_is_scalar(dst))
		return;

	if (dt_node_is_arith(dst) && dt_node_is_pointer(src) &&
	    (src->dn_flags & (DT_NF_ALLOCA | DT_NF_DPTR))) {
		int	mst;

		if ((mst = dt_regset_alloc(drp)) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

		dt_cg_access_dctx(mst, dlp, drp, DCTX_MST);
		emit(dlp,  BPF_STORE(BPF_DW, mst, DMST_SCALARIZER, src->dn_reg));
		emit(dlp,  BPF_LOAD(BPF_DW, dst->dn_reg, mst, DMST_SCALARIZER));

		dt_regset_free(drp, mst);
	} else {
		int	srcsigned = src->dn_flags & DT_NF_SIGNED;
		int	dstsigned = dst->dn_flags & DT_NF_SIGNED;
		size_t	srcsize = dt_node_type_size(src);
		size_t	dstsize = dt_node_type_size(dst);
		int	n = (sizeof(uint64_t) - dstsize) * NBBY;
		int	cast = 1;

		if (dst->dn_reg != src->dn_reg)
			emit(dlp, BPF_MOV_REG(dst->dn_reg, src->dn_reg));

		if (n == 0) {
			cast = 0;
		} else if (dstsize > srcsize) {
			if (dstsigned || !srcsigned)
				cast = 0;
		} else if (dstsize == srcsize) {
			if (dstsigned == srcsigned)
				cast = 0;
		}

		if (cast) {
			emit(dlp, BPF_ALU64_IMM(BPF_LSH, dst->dn_reg, n));
			emit(dlp, BPF_ALU64_IMM(dstsigned ? BPF_ARSH : BPF_RSH, dst->dn_reg, n));
		}
	}
}

/*
 * Generate code to push the specified argument list on to the tuple stack.
 * We use this routine for handling the index tuple for associative arrays
 * as well as keys for aggregations.
 *.
 * We must first generate code for all subexpressions because any subexpression
 * could itself require the use of the tuple assembly area and we only provide
 * one.
 *
 * Since the number of tuple components is unknown, we do not want to waste
 * registers on storing the subexpression values.  So, instead, we store the
 * subexpression values on the stack.
 *
 * Once code for all subexpressions has been generated, we assemble the tuple.
 *
 * Note that we leave space at the beginning of the tuple for a uint32_t value,
 * and at the end space for a uint64_t value.
 */
static void
dt_cg_arglist(dt_ident_t *idp, dt_node_t *args, dt_irlist_t *dlp,
	      dt_regset_t *drp)
{
	dtrace_hdl_t		*dtp = yypcb->pcb_hdl;
	const dt_idsig_t	*isp = idp->di_data;
	dt_node_t		*dnp;
	dt_ident_t		*maxtupsz = dt_dlib_get_var(dtp, "TUPSZ");
	int			i = 0;
	int			treg, areg;
	uint_t			tuplesize;

	TRACE_REGSET("      arglist: Begin");

	/* A 'void' args node indicates an empty argument list. */
	if (dt_node_is_void(args))
		goto empty_args;

	/*
	 * There is only one tuple assembly area, and computing a tuple member
	 * might itself require the tuple assembly area.  So, first compute
	 * tuple members (other than call stacks, which can take up a lot of
	 * space and can be computed at any time), saving them on the BPF stack.
	 */
	for (dnp = args; dnp != NULL; dnp = dnp->dn_list, i++) {
		/* Bail early if we run out of tuple slots. */
		if (i > dtp->dt_conf.dtc_diftupregs)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOTUPREG);

		/*
		 * Handle actions that can be used as agg keys.
		 * If we are not an aggregation, dt_cg_node(dnp, ...)
		 * will alert us there is a problem.
		 */
		if (idp->di_kind == DT_IDENT_AGG &&
		    dnp->dn_kind == DT_NODE_FUNC &&
		    dnp->dn_ident != NULL)
			switch (dnp->dn_ident->di_id) {
			case DT_ACT_STACK:
			case DT_ACT_USTACK:
				/* If this is a stack()-like function, we can handle it later. */
				dt_cg_push_stack(BPF_REG_FP, dlp, drp);
				continue;
			case DT_ACT_JSTACK:
				dnerror(dnp, D_UNKNOWN, "jstack() is not implemented (yet)\n");
				/* FIXME: Needs implementation */
			case DT_ACT_SYM:
			case DT_ACT_MOD:
			case DT_ACT_UADDR:
			case DT_ACT_UMOD:
			case DT_ACT_USYM:
				/* Otherwise, use the action's arg, not its "return value". */
				dt_cg_node(dnp->dn_args, dlp, drp);
				dt_cg_push_stack(dnp->dn_args->dn_reg, dlp, drp);
				dt_regset_free(drp, dnp->dn_args->dn_reg);
				continue;
			}

		/* Push the component (pointer or value) onto the tuple stack. */
		dt_cg_node(dnp, dlp, drp);
		dt_cg_push_stack(dnp->dn_reg, dlp, drp);
		dt_regset_free(drp, dnp->dn_reg);
	}

empty_args:
	TRACE_REGSET("      arglist: Stack");

	/*
	 * Get a pointer to the tuple assembly area.
	 */
	if ((treg = dt_regset_alloc(drp)) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	dt_cg_access_dctx(treg, dlp, drp, DCTX_MEM);
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, treg, DMEM_TUPLE(dtp)));

	/*
	 * We need to clear the tuple assembly area in case the previous tuple
	 * was larger than the one we will construct, because otherwise we end
	 * up with trailing garbage.
	 */
	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	emit(dlp,  BPF_MOV_REG(BPF_REG_1, treg));
	emite(dlp, BPF_MOV_IMM(BPF_REG_2, -1), maxtupsz);
	dt_cg_zerosptr(BPF_REG_3, dlp, drp);
	dt_regset_xalloc(drp, BPF_REG_0);
	emit(dlp,  BPF_CALL_HELPER(BPF_FUNC_probe_read));
	dt_regset_free_args(drp);
	dt_regset_free(drp, BPF_REG_0);

	/* Reserve space for a uint32_t value at the beginning of the tuple. */
	emit(dlp, BPF_ALU64_IMM(BPF_ADD, treg, sizeof(uint32_t)));
	tuplesize = sizeof(uint32_t);

	if ((areg = dt_regset_alloc(drp)) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	/* Determine first stack slot to read components from. */
	emit(dlp, BPF_LOAD(BPF_DW, areg, BPF_REG_FP, DT_STK_SP));
	emit(dlp, BPF_ALU64_IMM(BPF_ADD, areg, i * DT_STK_SLOT_SZ));

	/* Append each value to the tuple in the tuple assembly area. */
	for (dnp = args, i = 0; dnp != NULL; dnp = dnp->dn_list, i++) {
		dtrace_diftype_t	t;
		size_t			size;
		uint_t			nextoff;
		int			is_symmod = 0;

		if (dnp->dn_kind == DT_NODE_FUNC &&
		    dnp->dn_ident != NULL)
			switch (dnp->dn_ident->di_id) {
			case DT_ACT_STACK:
				tuplesize = dt_cg_act_stack_sub(yypcb, dnp, treg, tuplesize, DTRACEACT_STACK);
				continue;
			case DT_ACT_USTACK:
				tuplesize = dt_cg_act_stack_sub(yypcb, dnp, treg, tuplesize, DTRACEACT_USTACK);
				continue;
			case DT_ACT_UADDR:
			case DT_ACT_USYM:
			case DT_ACT_UMOD:
				nextoff = (tuplesize + (8 - 1)) & ~(8 - 1);
				if (tuplesize < nextoff)
					emit(dlp,  BPF_ALU64_IMM(BPF_ADD, treg, nextoff - tuplesize));

				/* Preface the value with the user process tgid. */
				if (dt_regset_xalloc_args(drp) == -1)
					longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
				dt_regset_xalloc(drp, BPF_REG_0);
				emit(dlp, BPF_CALL_HELPER(BPF_FUNC_get_current_pid_tgid));
				dt_regset_free_args(drp);
				emit(dlp, BPF_ALU64_IMM(BPF_AND, BPF_REG_0, 0xffffffff));
				emit(dlp, BPF_STORE(BPF_DW, treg, 0, BPF_REG_0));
				dt_regset_free(drp, BPF_REG_0);

				/* Then store the value. */
				dt_regset_xalloc(drp, BPF_REG_0);
				emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_0, areg, -i * DT_STK_SLOT_SZ));
				emit(dlp,  BPF_STORE(BPF_DW, treg, 8, BPF_REG_0));
				dt_regset_free(drp, BPF_REG_0);

				emit(dlp,  BPF_ALU64_IMM(BPF_ADD, treg, 16));
				tuplesize = nextoff + 16;

				continue;
			case DT_ACT_SYM:
			case DT_ACT_MOD:
				is_symmod = 1;
				size = 8;
				dt_regset_xalloc(drp, BPF_REG_0);
				emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_0, areg, -i * DT_STK_SLOT_SZ));
				break;
			}

		if (!is_symmod) {
			dt_node_diftype(dtp, dnp, &t);
			size = t.dtdt_size;
			if (size == 0)
				continue;

			dt_regset_xalloc(drp, BPF_REG_0);
			emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_0, areg, -i * DT_STK_SLOT_SZ));

			dnp->dn_reg = isp->dis_args[i].dn_reg = BPF_REG_0;
			dt_cg_typecast(dnp, &isp->dis_args[i], dlp, drp);
			isp->dis_args[i].dn_reg = -1;

			/* The typecast may have changed the size. */
			size = dt_node_sizeof(&isp->dis_args[i]);
		}

		if (dt_node_is_scalar(dnp) || dt_node_is_float(dnp) || is_symmod) {
			nextoff = (tuplesize + (size - 1)) & ~(size - 1);
			if (tuplesize < nextoff)
				emit(dlp,  BPF_ALU64_IMM(BPF_ADD, treg, nextoff - tuplesize));

			emit(dlp,  BPF_STOREX(size, treg, 0, BPF_REG_0));
			dt_regset_free(drp, BPF_REG_0);

			emit(dlp,  BPF_ALU64_IMM(BPF_ADD, treg, size));
			tuplesize = nextoff + size;
		} else if (dt_node_is_string(dnp)) {
			uint_t	lbl_valid = dt_irlist_label(dlp);

			if (dt_regset_xalloc_args(drp) == -1)
				longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

			emit(dlp,  BPF_MOV_REG(BPF_REG_1, treg));
			emit(dlp,  BPF_MOV_IMM(BPF_REG_2, size + 1));
			emit(dlp,  BPF_MOV_REG(BPF_REG_3, BPF_REG_0));
			dt_cg_tstring_free(yypcb, dnp);
			dt_regset_free(drp, BPF_REG_0);
			dt_regset_xalloc(drp, BPF_REG_0);
			emit(dlp,  BPF_CALL_HELPER(BPF_FUNC_probe_read_str));
			dt_regset_free_args(drp);
			emit(dlp,  BPF_BRANCH_IMM(BPF_JSGE, BPF_REG_0, 0, lbl_valid));
			dt_regset_free(drp, BPF_REG_0);
			dt_cg_probe_error(yypcb, DTRACEFLT_BADADDR,
					  DT_ISIMM, 128 + i);

			emitl(dlp, lbl_valid,
				   BPF_ALU64_IMM(BPF_ADD, treg, size + 1));
			tuplesize += size + 1;
		} else if (t.dtdt_flags & DIF_TF_BYREF) {
			uint_t	lbl_valid = dt_irlist_label(dlp);

			if (dt_regset_xalloc_args(drp) == -1)
				longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

			emit(dlp,  BPF_MOV_REG(BPF_REG_1, treg));
			emit(dlp,  BPF_MOV_IMM(BPF_REG_2, size));
			emit(dlp,  BPF_MOV_REG(BPF_REG_3, BPF_REG_0));
			dt_regset_free(drp, BPF_REG_0);
			dt_regset_xalloc(drp, BPF_REG_0);
			emit(dlp,  BPF_CALL_HELPER(BPF_FUNC_probe_read));
			dt_regset_free_args(drp);
			emit(dlp,  BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_0, 0, lbl_valid));
			dt_regset_free(drp, BPF_REG_0);
			dt_cg_probe_error(yypcb, DTRACEFLT_BADADDR,
					  DT_ISIMM, 0);

			emitl(dlp, lbl_valid,
				   BPF_ALU64_IMM(BPF_ADD, treg, size));
			tuplesize += size;
		} else
			assert(0);	/* We shouldn't be able to get here. */
	}

	/* Pop all components at once. */
	emit(dlp, BPF_STORE(BPF_DW, BPF_REG_FP, DT_STK_SP, areg));

	dt_regset_free(drp, areg);

	TRACE_REGSET("      arglist: Tuple");

	if (idp->di_flags & DT_IDFLG_TLS) {
		dt_ident_t	*idp = dt_dlib_get_func(dtp, "dt_tlskey");
		uint_t		varid = idp->di_id - DIF_VAR_OTHER_UBASE;

		assert(idp != NULL);

		if (dt_regset_xalloc_args(drp) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

		emit(dlp,  BPF_MOV_IMM(BPF_REG_1, varid));
		dt_regset_xalloc(drp, BPF_REG_0);
		emite(dlp, BPF_CALL_FUNC(idp->di_id), idp);
		dt_regset_free_args(drp);
		emit(dlp,  BPF_STORE(BPF_DW, treg, 0, BPF_REG_0));
		dt_regset_free(drp, BPF_REG_0);
	} else
		emit(dlp,  BPF_STORE_IMM(BPF_DW, treg, 0, 0));

	/* Account for the optional TLS key (or 0). */
	tuplesize += sizeof(uint64_t);

	if (tuplesize > dtp->dt_maxtuplesize)
		dtp->dt_maxtuplesize = tuplesize;

	/* Prepare the result as a pointer to the tuple assembly area. */
	dt_cg_access_dctx(treg, dlp, drp, DCTX_MEM);
	emit(dlp, BPF_ALU64_IMM(BPF_ADD, treg, DMEM_TUPLE(dtp)));

	args->dn_reg = treg;

	TRACE_REGSET("      arglist: End  ");
}

/*
 * dnp = node of the assignment
 *   dn_left = identifier node for the destination (idp = identifier)
 *   dn_right = value to store
 *
 * When dt_cg_store_var() is called, code generation for dnp->dn_right has
 * already been done and the result is in register dnp->dn_reg.
 */
static void
dt_cg_store_var(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp,
		dt_ident_t *idp)
{
	int	reg;
	size_t	size;

	idp->di_flags |= DT_IDFLG_DIFW;

	TRACE_REGSET("    store_var: Begin");

	/*
	 * Stores of DT_NF_NONALLOCA nodes into identifiers with DT_IDFLG_ALLOCA
	 * set indicate that an identifier has been reused for both alloca and
	 * non-alloca purposes.  Block this since it prevents us knowing whether
	 * to apply an offset to pointers loaded from this identifier.
	 */
	if (dnp->dn_flags & DT_NF_ALLOCA && idp->di_flags & DT_IDFLG_NONALLOCA) {
		xyerror(D_ALLOCA_INCOMPAT, "%s: cannot reuse the "
			"same identifier for both alloca and "
			"non-alloca allocations\n",
			idp->di_name);
	}

	/* First handle dvars. */
	if (idp->di_kind == DT_IDENT_ARRAY || idp->di_flags & DT_IDFLG_TLS) {
		uint_t		lbl_done = dt_irlist_label(dlp);
		uint_t		lbl_notnull = dt_irlist_label(dlp);
		dt_node_t	*rp = dnp->dn_right;

		dt_cg_prep_dvar(dnp, dlp, drp, idp, 1);

		/*
		 * Special case: a store of a literal 0 causes the dynamic
		 * variable to be deleted by the BPF function called above, so
		 * nothing is left to be done.
		 */
		if (rp->dn_kind == DT_NODE_INT && rp->dn_value == 0) {
			dt_regset_free(drp, BPF_REG_0);
			goto dvar_deleted;
		}

		emit(dlp,  BPF_BRANCH_IMM(BPF_JEQ, dnp->dn_reg, 0, lbl_done));
		emit(dlp,  BPF_BRANCH_IMM(BPF_JNE, BPF_REG_0, 0, lbl_notnull));
		emit(dlp,  BPF_MOV_IMM(BPF_REG_0, 0));
		emit(dlp,  BPF_RETURN());

		if ((reg = dt_regset_alloc(drp)) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

		emitl(dlp, lbl_notnull,
			   BPF_MOV_REG(reg, BPF_REG_0));
		dt_regset_free(drp, BPF_REG_0);

		size = idp->di_size;
		if (dnp->dn_flags & DT_NF_REF) {
			size_t	srcsz;

			/*
			 * Determine the amount of data to be copied.  It is
			 * the lesser of the size of the identifier and the
			 * size of the data being copied in.
			 */
			srcsz = dt_node_type_size(dnp->dn_right);
			size = MIN(srcsz, size);

			dt_cg_memcpy(dlp, drp, reg, dnp->dn_reg, size);
		} else
			emit(dlp, BPF_STOREX(size, reg, 0, dnp->dn_reg));

		dt_regset_free(drp, reg);

		emitl(dlp, lbl_done,
			   BPF_NOP());

dvar_deleted:
		TRACE_REGSET("    store_var: End  ");

		return;
	}

	/* global and local variables (that is, not thread-local) */
	if ((reg = dt_regset_alloc(drp)) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	/* get pointer to BPF map */
	if (idp->di_flags & DT_IDFLG_LOCAL)
		dt_cg_access_dctx(reg, dlp, drp, DCTX_LVARS);
	else
		dt_cg_access_dctx(reg, dlp, drp, DCTX_GVARS);

	/* Store */
	if (dt_node_is_string(dnp) && dt_node_is_integer(dnp->dn_right)) {
		/* Store a compile-time NULL pointer (string = NULL) */
		/*
		 * The parser has already ensured the rhs is 0.  In
		 * dt_cook_op2() for DT_TOK_ASGN, it checks dt_node_is_argcompat().
		 * Since rhs is not string compatible (string or char*), we move on
		 * to dt_node_is_ptrcompat(lp, rp, NULL, NULL).  There we see a test
		 * if (rp_is_int && (rp->dn_kind != DT_NODE_INT || rp->dn_value != 0))
		 * 	return 0; // fail if rp is an integer that is not 0 constant
		 */

		size = sizeof(DT_NULL_STRING);

		assert(dt_node_is_string(dnp) == dt_node_is_string(dnp->dn_left));
		assert(idp->di_size == dt_node_type_size(dnp->dn_left));
		assert(dt_node_is_integer(dnp->dn_right) == (dnp->dn_right->dn_kind == DT_NODE_INT));
		assert(dnp->dn_right->dn_value == 0);

		emit(dlp, BPF_ALU64_IMM(BPF_ADD, reg, idp->di_offset));
		emit(dlp, BPF_STOREX_IMM(size, reg, 0, DT_NULL_STRING));

		/*
		 * Since strings are passed by value, we need to force
		 * the value of the assignment to be the destination
		 * address.
		 */
		dt_regset_free(drp, dnp->dn_reg);
		dnp->dn_reg = reg;
	} else if (dt_node_is_string(dnp)) {
		/* General store to string */
		uint_t	Lnull = 0, Ldone = 0;
		size_t	srcsz;

		assert(dt_node_is_string(dnp) == dt_node_is_string(dnp->dn_left));
		assert(idp->di_size == dt_node_type_size(dnp->dn_left));
		assert(dnp->dn_reg == dnp->dn_right->dn_reg);

		emit(dlp, BPF_ALU64_IMM(BPF_ADD, reg, idp->di_offset));

		/* Check if we are storing a NULL */
		if (!(dnp->dn_right->dn_flags & DT_NF_ALLOCA)) {
			Lnull = dt_irlist_label(dlp);
			Ldone = dt_irlist_label(dlp);

			emit(dlp,  BPF_BRANCH_IMM(BPF_JEQ, dnp->dn_reg, 0, Lnull));
		}

		/* Normal store to string */

		/* Start by zeroing out the first bytes */
		size = sizeof(DT_NULL_STRING);

		emit(dlp, BPF_STOREX_IMM(size, reg, 0, 0));

		/*
		 * Determine the amount of data to be copied.  It is
		 * the lesser of the size of the identifier and the
		 * size of the data being copied in.
		 */
		srcsz = dt_node_type_size(dnp->dn_right);
		size = MIN(srcsz, idp->di_size);

		dt_cg_check_ptr_arg(dlp, drp, dnp->dn_right, NULL);

		dt_cg_memcpy(dlp, drp, reg, dnp->dn_reg, size);

		if (!(dnp->dn_right->dn_flags & DT_NF_ALLOCA)) {
			emit(dlp,  BPF_JUMP(Ldone));

			/* Store a null pointer */
			emitl(dlp, Lnull,
				   BPF_NOP());
			size = sizeof(DT_NULL_STRING);
			emit(dlp, BPF_STOREX_IMM(size, reg, 0, DT_NULL_STRING));
			emitl(dlp, Ldone,
				   BPF_NOP());
		}

		/*
		 * Since strings are passed by value, we need to force
		 * the value of the assignment to be the destination
		 * address.
		 */

		dt_regset_free(drp, dnp->dn_reg);
		dnp->dn_reg = reg;
	} else if (dnp->dn_flags & DT_NF_REF) {
		/* Store by reference (copy) */
		size_t		srcsz;

		emit(dlp, BPF_ALU64_IMM(BPF_ADD, reg, idp->di_offset));

		/*
		 * Determine the amount of data to be copied.  It is
		 * the lesser of the size of the identifier and the
		 * size of the data being copied in.
		 */
		srcsz = dt_node_type_size(dnp->dn_right);
		size = MIN(srcsz, idp->di_size);

		dt_cg_check_ptr_arg(dlp, drp, dnp->dn_right, NULL);
		dt_cg_memcpy(dlp, drp, reg, dnp->dn_reg, size);

		dt_regset_free(drp, reg);
	} else {
		/* Store by value */
		size = idp->di_size;

		emit(dlp, BPF_STOREX(size, reg, idp->di_offset, dnp->dn_reg));
		dt_regset_free(drp, reg);
	}

	TRACE_REGSET("    store_var: End  ");

	return;
}

static void
dt_cg_arithmetic_op(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp,
		    uint_t op)
{
	int is_ptr_op = (dnp->dn_op == DT_TOK_ADD || dnp->dn_op == DT_TOK_SUB ||
	    dnp->dn_op == DT_TOK_ADD_EQ || dnp->dn_op == DT_TOK_SUB_EQ);

	int lp_is_ptr = dt_node_is_pointer(dnp->dn_left);
	int rp_is_ptr = dt_node_is_pointer(dnp->dn_right);

	ctf_file_t	*ctfp = dnp->dn_left->dn_ctfp;
	ctf_id_t	type = ctf_type_resolve(ctfp, dnp->dn_left->dn_type);

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
		dt_cg_probe_error(yypcb, DTRACEFLT_DIVZERO, DT_ISIMM, 0);
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
	} else if (op == BPF_ADD
	    && dnp->dn_right->dn_kind != DT_NODE_INT
	    && ctf_type_kind(ctfp, type) == CTF_K_ARRAY) {
		/*
		 * If the left-hand argument is a CTF_K_ARRAY and we are adding
		 * an offset -- which is exactly what the parser will try to do
		 * if an element of a scalar array is being accessed -- then the
		 * BPF verifier will complain if it knows nothing about the offset.
		 * Meanwhile, if the offset is an DT_NODE_INT, then we have already
		 * checked it is in bounds.  So, add a run-time check only when
		 * the right-hand argument is not DT_NODE_INT.
		 */
		ctf_arinfo_t	r;
		uint_t		L1 = dt_irlist_label(dlp);
		ssize_t		elem_size;

		if (ctf_array_info(ctfp, type, &r) != 0) {
			yypcb->pcb_hdl->dt_ctferr = ctf_errno(ctfp);
			longjmp(yypcb->pcb_jmpbuf, EDT_CTF);
		}

		elem_size = ctf_type_size(ctfp, r.ctr_contents);

		/*
		 * Arrays of size 0 or 1 should not cause bounds checking as
		 * they are usually an anchor for dynamically sized arrays.
		 */
		if (r.ctr_nelems > 1) {
			emit(dlp,  BPF_BRANCH_IMM(BPF_JLT, dnp->dn_right->dn_reg, r.ctr_nelems * elem_size, L1));

			/* Report out-of-bounds fault on the index. */
			emit(dlp,  BPF_ALU64_IMM(BPF_DIV, dnp->dn_right->dn_reg, elem_size));
			dt_cg_probe_error(yypcb, DTRACEFLT_BADINDEX, DT_ISREG, dnp->dn_right->dn_reg);
		}

		emitl(dlp, L1,
			   BPF_ALU64_REG(op, dnp->dn_left->dn_reg, dnp->dn_right->dn_reg));
	} else
		emit(dlp,  BPF_ALU64_REG(op, dnp->dn_left->dn_reg, dnp->dn_right->dn_reg));

	dt_regset_free(drp, dnp->dn_right->dn_reg);
	dnp->dn_reg = dnp->dn_left->dn_reg;

	if (lp_is_ptr && rp_is_ptr)
		dt_cg_ptrsize(dnp->dn_right, dlp, drp, BPF_DIV, dnp->dn_reg);
}

static void
dt_cg_incdec_op(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp, uint_t op,
		int post)
{
	size_t		size = dt_node_type_size(dnp->dn_child);
	ssize_t		adj = 1;
	int		is_dvar = 0;
	uint_t		rbit = dnp->dn_child->dn_flags & DT_NF_REF;
	uint_t		lbl_done = DT_LBL_NONE;
	dt_ident_t	*idp = NULL;

	TRACE_REGSET("    incdec: Begin");

	if (dt_node_is_pointer(dnp)) {
		ctf_file_t	*ctfp = dnp->dn_ctfp;
		ctf_id_t	type;

		type = ctf_type_resolve(ctfp, dnp->dn_type);
		assert(ctf_type_kind(ctfp, type) == CTF_K_POINTER);
		adj = ctf_type_size(ctfp, ctf_type_reference(ctfp, type));
	}

	assert(dnp->dn_child->dn_flags & DT_NF_WRITABLE);
	assert(dnp->dn_child->dn_flags & DT_NF_LVALUE);

	dnp->dn_reg = dt_regset_alloc(drp);
	if (dnp->dn_reg == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	dnp->dn_child->dn_flags |= DT_NF_REF; /* force pass-by-ref */
	dt_cg_node(dnp->dn_child, dlp, drp);

	/*
	 * If the lvalue is a variable, we need to determine whether it is a
	 * dynamic variable (TLS or associative array element).  IF so, it may
	 * not have been created yet.  The lvalue pointer will be NULL in that
	 * case, and we need special handling for it.
	 *
	 * If the lvalue is not a variable, we need to perform a NULL check.
	 */
	if (dnp->dn_child->dn_kind == DT_NODE_VAR) {
		idp = dt_ident_resolve(dnp->dn_child->dn_ident);

		is_dvar = (idp->di_flags & DT_IDFLG_TLS ||
			   (idp->di_kind == DT_IDENT_ARRAY &&
			    idp->di_id > DIF_VAR_ARRAY_MAX));
	} else
		dt_cg_check_notnull(dlp, drp, dnp->dn_child->dn_reg);

	if (is_dvar) {
		dt_node_t	val;
		uint_t		lbl_dflt;

		/*
		 * The dt_cg_store_var() function expects a dnp->dn_right child
		 * so we fake one here.
		 */
		val.dn_op = DT_TOK_INT;
		val.dn_kind = DT_NODE_INT;
		val.dn_value = op == BPF_ADD ? adj : -adj;

		lbl_dflt = dt_irlist_label(dlp);
		lbl_done = dt_irlist_label(dlp);

		emit(dlp,  BPF_BRANCH_IMM(BPF_JNE, dnp->dn_child->dn_reg, 0, lbl_dflt));
		emit(dlp,  BPF_MOV_IMM(dnp->dn_reg, val.dn_value));

		dnp->dn_right = &val;
		dt_cg_store_var(dnp, dlp, drp, idp);
		dnp->dn_right = NULL;
		if (post)
			emit(dlp,  BPF_MOV_IMM(dnp->dn_reg, 0));
		emit(dlp,  BPF_JUMP(lbl_done));

		emitl(dlp, lbl_dflt,
			   BPF_NOP());
	}

	emit(dlp,  BPF_LOADX(size, dnp->dn_reg, dnp->dn_child->dn_reg, 0));
	if (post) {
		dt_regset_xalloc(drp, BPF_REG_0);
		emit(dlp,  BPF_MOV_REG(BPF_REG_0, dnp->dn_reg));
		emit(dlp,  BPF_ALU64_IMM(op, BPF_REG_0, adj));
		emit(dlp,  BPF_STOREX(size, dnp->dn_child->dn_reg, 0, BPF_REG_0));
		dt_regset_free(drp, BPF_REG_0);
	} else {
		emit(dlp,  BPF_ALU64_IMM(op, dnp->dn_reg, adj));
		emit(dlp,  BPF_STOREX(size, dnp->dn_child->dn_reg, 0, dnp->dn_reg));
	}

	dt_regset_free(drp, dnp->dn_child->dn_reg);

	if (lbl_done != DT_LBL_NONE)
		emitl(dlp, lbl_done,
			   BPF_NOP());

	dnp->dn_child->dn_flags &= ~DT_NF_REF;
	dnp->dn_child->dn_flags |= rbit;

	TRACE_REGSET("    incdec: End  ");
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
		/*
		 * String comparison looks at unsigned bytes, and
		 * "string op NULL" comparisons examine unsigned pointers.
		 */
		return 0;
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

	/* by now, we have checked and both args are strings or neither is */
	if (dt_node_is_string(dnp->dn_left) ||
	    dt_node_is_string(dnp->dn_right)) {
		dt_ident_t *idp;
		uint_t Lcomp = dt_irlist_label(dlp);
		uint32_t flags = 0;
		uint64_t off1, off2;

		/* if either pointer is NULL, just compare pointers */
		emit(dlp,  BPF_BRANCH_IMM(BPF_JEQ, dnp->dn_left->dn_reg, 0, Lcomp));
		emit(dlp,  BPF_BRANCH_IMM(BPF_JEQ, dnp->dn_right->dn_reg, 0, Lcomp));

		/*
		 * Set flags to indicate which arguments (if any) are pointers
		 * to DTrace-managed storage.
		 */
		if (dnp->dn_left->dn_flags & DT_NF_DPTR)
			flags |= 1;
		if (dnp->dn_right->dn_flags & DT_NF_DPTR)
			flags |= 2;

		/*
		 * Replace pointers with relative string values.
		 * Specifically, replace left with strcmp(left,right)+1 and
		 * right with 1, so we can use unsigned comparisons.
		 */

		off1 = dt_cg_tstring_xalloc(yypcb);
		off2 = dt_cg_tstring_xalloc(yypcb);

		if (dt_regset_xalloc_args(drp) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
		emit(dlp,  BPF_MOV_REG(BPF_REG_1, dnp->dn_left->dn_reg));
		emit(dlp,  BPF_MOV_REG(BPF_REG_2, dnp->dn_right->dn_reg));
		dt_cg_access_dctx(BPF_REG_3, dlp, drp, DCTX_MEM);
		emit(dlp,  BPF_MOV_REG(BPF_REG_4, BPF_REG_3));
		emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_3, off1));
		emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_4, off2));
		emit(dlp,  BPF_MOV_IMM(BPF_REG_5, flags));
		idp = dt_dlib_get_func(yypcb->pcb_hdl, "dt_strcmp");
		assert(idp != NULL);
		dt_regset_xalloc(drp, BPF_REG_0);
		emite(dlp, BPF_CALL_FUNC(idp->di_id), idp);
		dt_regset_free_args(drp);
		emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_0, 1));
		emit(dlp,  BPF_MOV_REG(dnp->dn_left->dn_reg, BPF_REG_0));
		dt_regset_free(drp, BPF_REG_0);
		emit(dlp,  BPF_MOV_IMM(dnp->dn_right->dn_reg, 1));

		dt_cg_tstring_xfree(yypcb, off1);
		dt_cg_tstring_xfree(yypcb, off2);

		dt_cg_tstring_free(yypcb, dnp->dn_left);
		dt_cg_tstring_free(yypcb, dnp->dn_right);

		/* proceed with the comparison */
		emitl(dlp, Lcomp,
			   BPF_NOP());
	}

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
	uint_t		lbl_false = dt_irlist_label(dlp);
	uint_t		lbl_post = dt_irlist_label(dlp);
	dt_irnode_t	*dip;

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

	/*
	 * Strings complicate things a bit because dn_left and dn_right might
	 * actually be temporary strings (tstring) *and* in different slots.
	 * We need to allocate a new tstring to hold the result, and copy the
	 * value into the new tstring (and free any tstrings in dn_left and
	 * dn_right).
	 */
	if (dt_node_is_string(dnp)) {
		uint_t lbl_null = dt_irlist_label(dlp);

		emit(dlp,  BPF_BRANCH_IMM(BPF_JEQ, dnp->dn_reg, 0, lbl_null));

		/*
		 * At this point, dnp->dn_reg holds a pointer to the string we
		 * need to copy.  But we want to copy it into a tstring which
		 * location is to be stored in dnp->dn_reg.  So, we need to
		 * shuffle things a bit.
		 */
		emit(dlp,  BPF_MOV_REG(BPF_REG_0, dnp->dn_reg));
		dt_cg_tstring_alloc(yypcb, dnp);

		dt_cg_access_dctx(dnp->dn_reg, dlp, drp, DCTX_MEM);
		emit(dlp,  BPF_ALU64_IMM(BPF_ADD, dnp->dn_reg, dnp->dn_tstring->dn_value));

		dt_cg_memcpy(dlp, drp, dnp->dn_reg, BPF_REG_0,
			     yypcb->pcb_hdl->dt_options[DTRACEOPT_STRSIZE]);

		emitl(dlp, lbl_null,
			   BPF_NOP());
		dt_cg_tstring_free(yypcb, dnp->dn_left);
		dt_cg_tstring_free(yypcb, dnp->dn_right);
	}
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

/*
 * When dt_cg_asgn_op() is called, code generation for dnp->dn_right has
 * already been done and the result is in register dnp->dn_reg.
 */
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

		dt_cg_store_var(dnp, dlp, drp, idp);

		/*
		 * Move the (possibly) tstring from dn_right to the op node.
		 */
		dnp->dn_tstring = dnp->dn_right->dn_tstring;
		dnp->dn_right->dn_tstring = NULL;
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
dt_cg_uregs(unsigned int idx, dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dtrace_hdl_t	*dtp = yypcb->pcb_hdl;

	/* check if out-of-bounds */
	if (idx >= sizeof(dt_pt_regs) / sizeof(uint64_t)) {

#if defined(__amd64)
		/* Confirm that we can hardwire for 21 pt_regs[]. */
		assert(sizeof(dt_pt_regs) == 21 * sizeof(uint64_t));

		/*
		 * Even if out-of-bounds, on x86 there are still a few
		 * indices that are used to access task->thread. members.
		 */
		if (idx <= 25) {
			int offset;
			ssize_t size;
			char *memnames[] = { "ds", "es", "fsbase", "gsbase", "trap_nr" };

			/* Look up task->thread offset. */
			offset = dt_cg_ctf_offsetof("struct task_struct", "thread", NULL, 0);

			/* Add the thread->member offset. */
			offset += dt_cg_ctf_offsetof("struct thread_struct",
						     memnames[idx - 21], &size, 0);

			/* Get task. */
			if (dt_regset_xalloc_args(drp) == -1)
				longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
			dt_regset_xalloc(drp, BPF_REG_0);
			emit(dlp, BPF_CALL_HELPER(BPF_FUNC_get_current_task));
			dt_regset_free_args(drp);
			emit(dlp, BPF_MOV_REG(dnp->dn_reg, BPF_REG_0));
			dt_regset_free(drp, BPF_REG_0);

			/* Add the offset to the task pointer. */
			emit(dlp, BPF_ALU64_IMM(BPF_ADD, dnp->dn_reg, offset));

			/* Dereference it safely (the BPF verifier has no idea what it is). */
			dt_cg_load_scalar(dnp, BPF_SZ_NONE, size, dlp, drp);

			return;
		}
#endif

		dnerror(dnp, D_UNKNOWN, "uregs[]: index out of bounds (%d)\n", idx);

		return;
	}

	/* If in-bounds, look up pt_regs[]. */

	if (dtp->dt_bpfhelper[BPF_FUNC_get_current_task_btf] == BPF_FUNC_unspec ||
	    dtp->dt_bpfhelper[BPF_FUNC_task_pt_regs] == BPF_FUNC_unspec) {

		/*
		 * To get pt_regs[], we try to use two BPF helper functions, but
		 * they exist only on newer kernels.  Therefore, we check if they
		 * exist.
		 *
		 * If they do not exist, we are forced to emulate them, which is
		 * tricky since they depend on kernel configuration.
		 *
		 * In kernel/trace/bpf_trace.c, we see bpf_task_pt_regs() returns
		 * task_pt_regs(task).
		 *
		 * In turn, task_pt_regs() is defined in files like
		 *     arch/arm64/include/asm/processor.h
		 *     arch/x86/include/asm/processor.h
		 *
		 * In essence, what we will do for these two architectures is:
		 *     get task_stack_page(task) (that is, get task->stack)
		 *     find the offset to add:
		 *         add THREAD_SIZE
		 *         subtract sizeof(struct pt_regs)
		 *         add idx*sizeof(uint64_t) (for pt_regs[idx])
		 *
		 * For a wide range of kernels, the configuration parameters will
		 * be identical to the values below.
		 */

		size_t offset;

		/* Spill %r0 - %r5 (if necessary). */
		if (dt_regset_xalloc_args(drp) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
		dt_regset_xalloc(drp, BPF_REG_0);

		/* %r0 = current stack */
		emit(dlp, BPF_CALL_HELPER(BPF_FUNC_get_current_task));

		/* Copy contents at task->stack to %fp+DT_STK_SP (scratch space). */
		emit(dlp, BPF_MOV_REG(BPF_REG_3, BPF_REG_0));
		emit(dlp, BPF_ALU64_IMM(BPF_ADD, BPF_REG_3,
		    dt_cg_ctf_offsetof("struct task_struct", "stack", NULL, 0)));
		emit(dlp, BPF_MOV_IMM(BPF_REG_2, sizeof(uint64_t)));
		emit(dlp, BPF_LOAD(BPF_DW, BPF_REG_1, BPF_REG_FP, DT_STK_SP));
		emit(dlp, BPF_CALL_HELPER(dtp->dt_bpfhelper[BPF_FUNC_probe_read_kernel]));

		/* Fill %r0 - %r5 (if necessary). */
		dt_regset_free_args(drp);
		dt_regset_free(drp, BPF_REG_0);

		/* Get contents from %fp+DT_STK_SP.  Now we have task->stack. */
		emit(dlp, BPF_LOAD(BPF_DW, dnp->dn_reg, BPF_REG_FP, DT_STK_SP));
		emit(dlp, BPF_LOAD(BPF_DW, dnp->dn_reg, dnp->dn_reg, 0));

		/* Start computing the offset to get to pt_regs[idx]. */
		offset = 0;

#if defined(__amd64)

#define MY_PAGE_SIZE 4096

/* arch/x86/include/asm/page_64_types.h */
#define MY_KASAN_STACK_ORDER 0                              /* for CONFIG_KASAN not set */
#define MY_THREAD_SIZE_ORDER (2 + MY_KASAN_STACK_ORDER)
#define MY_THREAD_SIZE (MY_PAGE_SIZE << MY_THREAD_SIZE_ORDER)

		offset += MY_THREAD_SIZE;

#undef MY_PAGE_SIZE
#undef MY_KASAN_STACK_ORDER
#undef MY_THREAD_SIZE_ORDER
#undef MY_THREAD_SIZE

#elif defined(__aarch64__)
		{
			int tmp = getpagesize(), page_shift = 0;
			int kasan_thread_shift, min_thread_shift, thread_shift, thread_size;

			/* Discover PAGE_SHIFT, set in arch/arm64/include/asm/page-def.h */
			while (tmp > 1) {
				page_shift++;
				tmp >>= 1;
			}

			/* Work through logic of arch/arm64/include/asm/memory.h */
			kasan_thread_shift = 0;                     /* for CONFIG_KASAN not set */
			min_thread_shift = 14 + kasan_thread_shift;
			if (min_thread_shift < page_shift)          /* for CONFIG_VMAP_STACK */
				thread_shift = page_shift;
			else
				thread_shift = min_thread_shift;
			thread_size = 1 << thread_shift;

			/* Now we have the offset. */
			offset += thread_size;
		}
#else
# error ISA not supported
#endif

		/*
		 * Subtract sizeof(struct pt_regs).  Do not use sizeof(dt_pt_regs)
		 * since it can be smaller, at least on aarch64.
		 */
		{
			ctf_file_t *cfp = yypcb->pcb_hdl->dt_shared_ctf;
			ctf_id_t type;

			type = ctf_lookup_by_name(cfp, "struct pt_regs");
			if (type == CTF_ERR)
				longjmp(yypcb->pcb_jmpbuf, EDT_NOCTF);

			offset -= ctf_type_size(cfp, type);
		}

		/* Add the offset for our particular pt_regs[] index. */
		offset += (idx * sizeof(uint64_t));

		/* Add the fully computed offset to the pointer in the register. */
		emit(dlp, BPF_ALU64_IMM(BPF_ADD, dnp->dn_reg, offset));

		/* Dereference it safely (the BPF verifier has no idea what it is). */
		dt_cg_load_scalar(dnp, BPF_DW, sizeof(uint64_t), dlp, drp);

		return;
	}

	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	dt_regset_xalloc(drp, BPF_REG_0);
	emit(dlp, BPF_CALL_HELPER(BPF_FUNC_get_current_task_btf));
	emit(dlp, BPF_MOV_REG(BPF_REG_1, BPF_REG_0));
	emit(dlp, BPF_CALL_HELPER(BPF_FUNC_task_pt_regs));
	dt_regset_free_args(drp);
	/* The BPF verifier allows this load since it tracks %r0 as "ptr_pt_regs". */
	emit(dlp, BPF_LOAD(BPF_DW, dnp->dn_reg, BPF_REG_0, idx * sizeof(uint64_t)));
	dt_regset_free(drp, BPF_REG_0);
}

/*
 * Currently, this code is only used for the args[] and uregs[] arrays.
 */
static void
dt_cg_array_op(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_probe_t	*prp = yypcb->pcb_probe;
	uintmax_t	saved = dnp->dn_args->dn_value;
	dt_ident_t	*idp = dnp->dn_ident;
	dt_ident_t	*fidp = dt_dlib_get_func(yypcb->pcb_hdl, "dt_get_bvar");
	size_t		size;
	int		n;
	int		ustr = 0;

	assert(dnp->dn_kind == DT_NODE_VAR);
	assert(!(idp->di_flags & (DT_IDFLG_TLS | DT_IDFLG_LOCAL)));

	assert(dnp->dn_args->dn_kind == DT_NODE_INT);
	assert(dnp->dn_args->dn_list == NULL);

	assert(fidp != NULL);

	/*
	 * If this is a reference in the args[] array, temporarily modify the
	 * array index according to the static argument mapping (if any),
	 * unless the argument reference is provided by a dynamic translator.
	 * If we're using a dynamic translator for args[], then just set dn_reg
	 * to an invalid reg and return: DIF_OP_XLARG will fetch the arg later.
	 *
	 * If this is a userland variable, note that we need to copy it in.
	 */
	if (idp->di_id == DIF_VAR_ARGS) {
		if ((idp->di_kind == DT_IDENT_XLPTR ||
		    idp->di_kind == DT_IDENT_XLSOU) &&
		    dt_xlator_dynamic(idp->di_data)) {
			dnp->dn_reg = -1;
			return;
		}

		/*
		 * If the argument node is mark DT_NF_USERLAND, it is a string
		 * in userspace.  Set a flag to indicate we need to copy it.
		 */
		ustr = prp->xargv[saved]->dn_flags & DT_NF_USERLAND;
		dnp->dn_args->dn_value = prp->mapping[saved];
	}

	/*
	 * Mark the identifier as referenced by the current DIFO.  If it is an
	 * orphaned identifier, we also need to mark the primary identifier
	 * (global identifier with the same name).
	 *
	 * We can safely replace idp with the primary identifier at this point
	 * because they have the same id.
	 */
	idp->di_flags |= DT_IDFLG_DIFR;
	if (idp->di_flags & DT_IDFLG_ORPHAN) {
		idp = dt_idhash_lookup(yypcb->pcb_hdl->dt_globals, "args");
		assert(idp != NULL);
		idp->di_flags |= DT_IDFLG_DIFR;
	}

	dt_cg_node(dnp->dn_args, dlp, drp);
	dnp->dn_args->dn_value = saved;
	dnp->dn_reg = dnp->dn_args->dn_reg;

	if (idp->di_id == DIF_VAR_UREGS) {
		/*
		 * We can use dnp->dn_args->dn_value, even though it was
		 * just overwritten by "saved", because we know the index
		 * is is a constant integer DT_NODE_INT.
		 */
		dt_cg_uregs(dnp->dn_args->dn_value, dnp, dlp, drp);
		return;
	}

	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	dt_cg_access_dctx(BPF_REG_1, dlp, drp, -1);
	emit(dlp, BPF_MOV_IMM(BPF_REG_2, idp->di_id));
	emit(dlp, BPF_MOV_REG(BPF_REG_3, dnp->dn_reg));
	dt_regset_xalloc(drp, BPF_REG_0);
	emite(dlp, BPF_CALL_FUNC(fidp->di_id), fidp);
	dt_regset_free_args(drp);

	dt_cg_check_fault(yypcb);

	emit(dlp, BPF_MOV_REG(dnp->dn_reg, BPF_REG_0));
	dt_regset_free(drp, BPF_REG_0);

	if (dt_node_is_string(dnp) && ustr) {
		dtrace_hdl_t    *dtp = yypcb->pcb_hdl;

		if (dt_regset_xalloc_args(drp) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

		emit(dlp,  BPF_MOV_REG(BPF_REG_3, dnp->dn_reg));
		emit(dlp,  BPF_MOV_IMM(BPF_REG_2, dtp->dt_options[DTRACEOPT_STRSIZE]));
		dt_cg_tstring_alloc(yypcb, dnp);
		dt_cg_access_dctx(dnp->dn_reg, dlp, drp, DCTX_MEM);
		emit(dlp,  BPF_ALU64_IMM(BPF_ADD, dnp->dn_reg, dnp->dn_tstring->dn_value));

		emit(dlp,  BPF_MOV_REG(BPF_REG_1, dnp->dn_reg));
		dt_regset_xalloc(drp, BPF_REG_0);
		emit(dlp,  BPF_CALL_HELPER(dtp->dt_bpfhelper[BPF_FUNC_probe_read_user_str]));
		dt_regset_free(drp, BPF_REG_0);
		dt_regset_free_args(drp);

		return;
	}

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
	emit(dlp, BPF_ALU64_IMM((dnp->dn_flags & DT_NF_SIGNED) ? BPF_ARSH : BPF_RSH, dnp->dn_reg, n));
}

/*
 * Emit code to call a precompiled BPF function (named by fname)
 * that is of return type void and takes between three and five arguments:
 *   - a pointer to the DTrace context (dctx)
 *   - one input value
 *   - a pointer to an output tstring, allocated here
 *   - two optional uint32_t values
 */
static void
dt_cg_subr_arg_to_tstring(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp,
			  const char *fname, int argnum, int isreg4,
			  uint32_t val4, int isreg5, uint32_t val5)
{
	dt_ident_t	*idp;
	dt_node_t	*arg = dnp->dn_args;
	dt_idsig_t	*isp;
	dt_node_t	*argtype;

	TRACE_REGSET("    subr-arg_to_tstring:Begin");

	assert(dnp->dn_ident);
	isp = dnp->dn_ident->di_data;
	assert(isp && isp->dis_args);
	assert(argnum < isp->dis_argc);
	argtype = &isp->dis_args[argnum];

	/* handle the one "input value" */
	/* (its type matters only as to whether we check it is null */
	while (argnum-- > 0) {
		assert(arg != NULL);
		arg = arg->dn_list;
	}
	dt_cg_node(arg, dlp, drp);
	if (dt_node_is_pointer(argtype) || dt_node_is_string(argtype))
		dt_cg_check_ptr_arg(dlp, drp, arg, NULL);

	/* allocate the temporary string */
	dnp->dn_reg = dt_regset_alloc(drp);
	if (dnp->dn_reg == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	dt_cg_tstring_alloc(yypcb, dnp);
	dt_cg_access_dctx(dnp->dn_reg, dlp, drp, DCTX_MEM);
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, dnp->dn_reg, dnp->dn_tstring->dn_value));

	/* function call */
	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	emit(dlp, BPF_MOV_REG(BPF_REG_2, arg->dn_reg));
	dt_regset_free(drp, arg->dn_reg);
	if (dt_node_is_string(arg))
		dt_cg_tstring_free(yypcb, arg);

	emit(dlp,  BPF_MOV_REG(BPF_REG_3, dnp->dn_reg));

	if (isreg4 == DT_ISREG)
		emit(dlp, BPF_MOV_REG(BPF_REG_4, val4));
	else if (isreg4 == DT_ISIMM)
		emit(dlp, BPF_MOV_IMM(BPF_REG_4, val4));

	if (isreg5 == DT_ISREG)
		emit(dlp, BPF_MOV_REG(BPF_REG_5, val5));
	else if (isreg5 == DT_ISIMM)
		emit(dlp, BPF_MOV_IMM(BPF_REG_5, val5));

	dt_cg_access_dctx(BPF_REG_1, dlp, drp, -1);

	idp = dt_dlib_get_func(yypcb->pcb_hdl, fname);
	assert(idp != NULL);
	dt_regset_xalloc(drp, BPF_REG_0);
	emite(dlp,  BPF_CALL_FUNC(idp->di_id), idp);
	dt_regset_free_args(drp);
	dt_regset_free(drp, BPF_REG_0);

	TRACE_REGSET("    subr-arg_to_tstring:End  ");
}

static void
dt_cg_subr_basename(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_cg_subr_arg_to_tstring(dnp, dlp, drp, "dt_basename", 0, DT_IGNOR, 0,
				  DT_IGNOR, 0);
}

static void
dt_cg_subr_cleanpath(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_cg_subr_arg_to_tstring(dnp, dlp, drp, "dt_cleanpath", 0, DT_IGNOR, 0,
				  DT_IGNOR, 0);
}

static void
dt_cg_subr_dirname(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_cg_subr_arg_to_tstring(dnp, dlp, drp, "dt_dirname", 0, DT_IGNOR, 0,
				  DT_IGNOR, 0);
}

static void
dt_cg_subr_index(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_node_t	*s = dnp->dn_args;
	dt_node_t	*t = s->dn_list;
	dt_node_t	*start = t->dn_list;
	dt_ident_t	*idp = dt_dlib_get_func(yypcb->pcb_hdl, "dt_index");
	uint64_t	off1, off2;

	assert(idp != NULL);

	TRACE_REGSET("    subr-index:Begin");

	dt_cg_node(s, dlp, drp);
	dt_cg_check_ptr_arg(dlp, drp, s, NULL);
	dt_cg_node(t, dlp, drp);
	dt_cg_check_ptr_arg(dlp, drp, t, NULL);
	if (start != NULL)
		dt_cg_node(start, dlp, drp);

	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	emit(dlp,  BPF_MOV_REG(BPF_REG_1, s->dn_reg));
	dt_regset_free(drp, s->dn_reg);
	dt_cg_tstring_free(yypcb, s);
	emit(dlp,  BPF_MOV_REG(BPF_REG_2, t->dn_reg));
	dt_regset_free(drp, t->dn_reg);
	dt_cg_tstring_free(yypcb, t);
	if (start) {
		emit(dlp,  BPF_MOV_REG(BPF_REG_3, start->dn_reg));
		dt_regset_free(drp, start->dn_reg);
	} else
		emit(dlp,  BPF_MOV_IMM(BPF_REG_3, 0));

	off1 = dt_cg_tstring_xalloc(yypcb);
	off2 = dt_cg_tstring_xalloc(yypcb);

	dt_cg_access_dctx(BPF_REG_4, dlp, drp, DCTX_MEM);
	emit(dlp,  BPF_MOV_REG(BPF_REG_5, BPF_REG_4));
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_4, off1));
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_5, off2));

	dt_regset_xalloc(drp, BPF_REG_0);
	emite(dlp, BPF_CALL_FUNC(idp->di_id), idp);
	dt_regset_free_args(drp);

	dt_cg_tstring_xfree(yypcb, off1);
	dt_cg_tstring_xfree(yypcb, off2);

	dnp->dn_reg = dt_regset_alloc(drp);
	if (dnp->dn_reg == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	emit(dlp,  BPF_MOV_REG(dnp->dn_reg, BPF_REG_0));
	dt_regset_free(drp, BPF_REG_0);

	TRACE_REGSET("    subr-index:End  ");
}

static void
dt_cg_subr_lltostr(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_cg_subr_arg_to_tstring(dnp, dlp, drp, "dt_lltostr", 0, DT_IGNOR, 0,
				  DT_IGNOR, 0);
}

static void
dt_cg_subr_progenyof(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_ident_t	*idp = dt_dlib_get_func(yypcb->pcb_hdl, "dt_progenyof");
	dt_node_t	*arg = dnp->dn_args;

	assert(idp != NULL);

	TRACE_REGSET("    subr-progenyof:Begin");
	dt_cg_node(arg, dlp, drp);

	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	emit(dlp, BPF_MOV_REG(BPF_REG_1, arg->dn_reg));
	dt_regset_free(drp, arg->dn_reg);
	dt_regset_xalloc(drp, BPF_REG_0);
	emite(dlp,  BPF_CALL_FUNC(idp->di_id), idp);
	dt_regset_free_args(drp);

	dnp->dn_reg = dt_regset_alloc(drp);
	if (dnp->dn_reg == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	emit(dlp, BPF_MOV_REG(dnp->dn_reg, BPF_REG_0));
	dt_regset_free(drp, BPF_REG_0);

	TRACE_REGSET("    subr-progenyof:End  ");
}

static void
dt_cg_subr_rand(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	TRACE_REGSET("    subr-rand:Begin");

	dnp->dn_reg = dt_regset_alloc(drp);
	if (dnp->dn_reg == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	dt_regset_xalloc(drp, BPF_REG_0);
	emit(dlp,  BPF_CALL_HELPER(BPF_FUNC_get_prandom_u32));
	dt_regset_free_args(drp);
	emit(dlp,  BPF_MOV_REG(dnp->dn_reg, BPF_REG_0));
	dt_regset_free(drp, BPF_REG_0);

	emit(dlp,  BPF_ALU64_IMM(BPF_LSH, dnp->dn_reg, 32));
	emit(dlp,  BPF_ALU64_IMM(BPF_ARSH, dnp->dn_reg, 32));

	TRACE_REGSET("    subr-rand:End  ");
}

/*
 * This function is a helper function for subroutines that take some
 * (8-byte) temporary space and a lock argument, returning the value
 * from a given BPF function (passed by name).
 */
static void
dt_cg_subr_lock_helper(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp,
		       const char *fname)
{
	dt_ident_t	*idp = dt_dlib_get_func(yypcb->pcb_hdl, fname);
	dt_node_t	*lock = dnp->dn_args;
	uint64_t	off = dt_cg_tstring_xalloc(yypcb);

	assert(idp != NULL);

	TRACE_REGSET("    subr-lock_helper:Begin");

	dt_cg_node(lock, dlp, drp);
	dt_cg_check_notnull(dlp, drp, lock->dn_reg);

	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	dt_cg_access_dctx(BPF_REG_1, dlp, drp, DCTX_MEM);
	emit(dlp, BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, off));

	emit(dlp, BPF_MOV_REG(BPF_REG_2, lock->dn_reg));
	dt_regset_free(drp, lock->dn_reg);

	dt_regset_xalloc(drp, BPF_REG_0);
	emite(dlp,  BPF_CALL_FUNC(idp->di_id), idp);
	dt_regset_free_args(drp);
	dt_cg_tstring_xfree(yypcb, off);

	dnp->dn_reg = dt_regset_alloc(drp);
	if (dnp->dn_reg == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	emit(dlp, BPF_MOV_REG(dnp->dn_reg, BPF_REG_0));
	dt_regset_free(drp, BPF_REG_0);

	TRACE_REGSET("    subr-lock_helper:End  ");
}

static void
dt_cg_subr_mutex_owned(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_cg_subr_lock_helper(dnp, dlp, drp, "dt_mutex_owned");
}

static void
dt_cg_subr_mutex_owner(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_cg_subr_lock_helper(dnp, dlp, drp, "dt_mutex_owner");
}

static void
dt_cg_subr_rw_read_held(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_cg_subr_lock_helper(dnp, dlp, drp, "dt_rw_read_held");
}

static void
dt_cg_subr_rw_write_held(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_cg_subr_lock_helper(dnp, dlp, drp, "dt_rw_write_held");
}

static void
dt_cg_subr_rw_iswriter(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_cg_subr_lock_helper(dnp, dlp, drp, "dt_rw_iswriter");
}

/* On Linux, all mutexes are adaptive. */

static void
dt_cg_subr_mutex_type_adaptive(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_node_t	*arg = dnp->dn_args;

	TRACE_REGSET("    subr-mutex_type_adaptive:Begin");
	dt_cg_node(arg, dlp, drp);
	dnp->dn_reg = arg->dn_reg;
	emit(dlp,  BPF_MOV_IMM(dnp->dn_reg, 1));
	TRACE_REGSET("    subr-mutex_type_adaptive:End  ");
}

static void
dt_cg_subr_mutex_type_spin(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_node_t	*arg = dnp->dn_args;

	TRACE_REGSET("    subr-mutex_type_spin:Begin");
	dt_cg_node(arg, dlp, drp);
	dnp->dn_reg = arg->dn_reg;
	emit(dlp,  BPF_MOV_IMM(dnp->dn_reg, 0));
	TRACE_REGSET("    subr-mutex_type_spin:End  ");
}

static void
dt_cg_subr_rindex(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_node_t	*s = dnp->dn_args;
	dt_node_t	*t = s->dn_list;
	dt_node_t	*start = t->dn_list;
	dt_ident_t	*idp = dt_dlib_get_func(yypcb->pcb_hdl, "dt_rindex");
	uint64_t	off1, off2;
	uint_t		Lneg_start = dt_irlist_label(dlp);

	assert(idp != NULL);

	TRACE_REGSET("    subr-rindex:Begin");

	/* evaluate arguments to D subroutine rindex() */
	dt_cg_node(s, dlp, drp);
	dt_cg_check_ptr_arg(dlp, drp, s, NULL);
	dt_cg_node(t, dlp, drp);
	dt_cg_check_ptr_arg(dlp, drp, t, NULL);
	if (start != NULL)
		dt_cg_node(start, dlp, drp);

	/* start setting up function call to BPF function dt_rindex() */
	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	/* string s and substring t */
	emit(dlp,  BPF_MOV_REG(BPF_REG_1, s->dn_reg));
	dt_regset_free(drp, s->dn_reg);
	dt_cg_tstring_free(yypcb, s);
	emit(dlp,  BPF_MOV_REG(BPF_REG_2, t->dn_reg));
	dt_regset_free(drp, t->dn_reg);
	dt_cg_tstring_free(yypcb, t);

	/* scratch memory */
	off1 = dt_cg_tstring_xalloc(yypcb);
	off2 = dt_cg_tstring_xalloc(yypcb);

	dt_cg_access_dctx(BPF_REG_4, dlp, drp, DCTX_MEM);
	emit(dlp,  BPF_MOV_REG(BPF_REG_5, BPF_REG_4));
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_4, off1));
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_5, off2));

	/* allocate return register */
	dt_regset_xalloc(drp, BPF_REG_0);

	/*
	 * Now deal with the "start" argument.  If it's negative, we will
	 * bypass the function call.  That means we set all those other
	 * arguments up for nothing, but doing it this way simplifies
	 * code generation and start<0 should be a rare condition.
	 *
	 * If there is no "start" argument, then specify -1.  That is,
	 * we are modifying the semantics of start<0 for internal purposes.
	 */
	if (start) {
		emit(dlp,  BPF_MOV_IMM(BPF_REG_0, -1));
		emit(dlp,  BPF_BRANCH_IMM(BPF_JSLT, start->dn_reg, 0, Lneg_start));
		emit(dlp,  BPF_MOV_REG(BPF_REG_3, start->dn_reg));
		dt_regset_free(drp, start->dn_reg);
	} else
		emit(dlp,  BPF_MOV_IMM(BPF_REG_3, -1));

	/* function call */
	emite(dlp, BPF_CALL_FUNC(idp->di_id), idp);
	emitl(dlp, Lneg_start,
		   BPF_NOP());
	dt_regset_free_args(drp);

	/* clean up */
	dt_cg_tstring_xfree(yypcb, off1);
	dt_cg_tstring_xfree(yypcb, off2);

	dnp->dn_reg = dt_regset_alloc(drp);
	if (dnp->dn_reg == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	emit(dlp,  BPF_MOV_REG(dnp->dn_reg, BPF_REG_0));
	dt_regset_free(drp, BPF_REG_0);

	TRACE_REGSET("    subr-rindex:End  ");
}

/*
 * For getmajor and getminor, use MAJOR(dev) and MINOR(dev)
 * as defined in kernel header include/linux/kdev_t.h, not
 * as defined in user header /usr/include/linux/kdev_t.h.
 */
static void
dt_cg_subr_getmajor(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_node_t	*arg = dnp->dn_args;

	dt_cg_node(arg, dlp, drp);
	dnp->dn_reg = arg->dn_reg;
	emit(dlp, BPF_ALU64_IMM(BPF_LSH, dnp->dn_reg, 32));
	emit(dlp, BPF_ALU64_IMM(BPF_RSH, dnp->dn_reg, 32 + 20));
}

static void
dt_cg_subr_getminor(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_node_t	*arg = dnp->dn_args;

	dt_cg_node(arg, dlp, drp);
	dnp->dn_reg = arg->dn_reg;
	emit(dlp, BPF_ALU64_IMM(BPF_AND, dnp->dn_reg, 0xfffff));
}

/*
 * Get and return a new speculation ID.  These are unallocated entries in the
 * specs map, obtained by calling dt_speculation().  Return zero if none is
 * available.  TODO: add a drop in this case?
 */
static void
dt_cg_subr_speculation(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_ident_t	*idp;

	idp = dt_dlib_get_func(yypcb->pcb_hdl, "dt_speculation");
	assert(idp != NULL);

	TRACE_REGSET("    subr-speculation:Begin");
	if ((dnp->dn_reg = dt_regset_alloc(drp)) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);


	/*
	 *	spec = dt_speculation();
	 *				// call dt_speculation
	 *				//     (%r1 ... %r5 clobbered)
	 *				//     (%r0 = speculation ID, or 0)
	 *	return rc;		// mov %dn_reg, %r0
	 */

	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	dt_regset_xalloc(drp, BPF_REG_0);
	emite(dlp, BPF_CALL_FUNC(idp->di_id), idp);
	emit(dlp,  BPF_MOV_REG(dnp->dn_reg, BPF_REG_0));
	dt_regset_free(drp, BPF_REG_0);
	dt_regset_free_args(drp);
	TRACE_REGSET("    subr-speculation:End  ");
}

static void
dt_cg_subr_alloca_impl(dt_node_t *dnp, dt_node_t *size, dt_irlist_t *dlp,
		       dt_regset_t *drp)
{
	uint_t		lbl_ok = dt_irlist_label(dlp);
	uint_t		lbl_err = dt_irlist_label(dlp);
	int		opt_scratchsize = yypcb->pcb_hdl->dt_options[DTRACEOPT_SCRATCHSIZE];
	int		mst, next;

	TRACE_REGSET("      subr-alloca-impl:Begin");

	/*
	 * Compile-error out if the size is too large even absent other
	 * allocations.  (This prevents us generating code for which the
	 * verifier will fail due to one branch of the conditional below being
	 * determined to be unreachable.)
	 *
	 * We need to adjust the scratchsize to account for the first 8 bytes
	 * that are used to represent the NULL pointer.
	 */
	if (size->dn_kind == DT_NODE_INT &&
	    size->dn_value > opt_scratchsize - 8)
		dnerror(dnp, D_ALLOCA_SIZE,
			"alloca(%lu) size larger than scratchsize %lu\n",
			(unsigned long) size->dn_value,
			(unsigned long) opt_scratchsize - 8);

	if (((mst = dt_regset_alloc(drp)) == -1) ||
	    ((next = dt_regset_alloc(drp)) == -1))
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	/* Loading.  */

	dt_cg_access_dctx(mst, dlp, drp, DCTX_MST);
	emit(dlp,  BPF_LOAD(BPF_W, next, mst, DMST_SCRATCH_TOP));

	/* Size testing and alignment.  */

	emit(dlp,  BPF_MOV_REG(dnp->dn_reg, next));
	emit(dlp,  BPF_ALU64_REG(BPF_ADD, next, size->dn_reg));
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, next, 7));
	emit(dlp,  BPF_ALU64_IMM(BPF_AND, next, (int) -8));

	emit(dlp,  BPF_BRANCH_IMM(BPF_JGT, next, opt_scratchsize, lbl_err));
	emit(dlp,  BPF_BRANCH_IMM(BPF_JLE, dnp->dn_reg, opt_scratchsize - 8,
				  lbl_ok));
	emitl(dlp, lbl_err,
		   BPF_NOP());
	dt_cg_probe_error(yypcb, DTRACEFLT_NOSCRATCH, DT_ISIMM, 0);
	emitl(dlp, lbl_ok,
		   BPF_STORE(BPF_W, mst, DMST_SCRATCH_TOP, next));

	dt_regset_free(drp, mst);
	dt_regset_free(drp, next);

	TRACE_REGSET("      subr-alloca-impl:End  ");
}

static void
dt_cg_subr_alloca(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_node_t	*size = dnp->dn_args;

	TRACE_REGSET("    subr-alloca:Begin");

	dt_cg_node(size, dlp, drp);

	if ((dnp->dn_reg = dt_regset_alloc(drp)) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	dt_cg_subr_alloca_impl(dnp, size, dlp, drp);

	dt_regset_free(drp, size->dn_reg);

	TRACE_REGSET("    subr-alloca:End  ");
}

static void
dt_cg_subr_bcopy_impl(dt_node_t *dnp, dt_node_t *dst, dt_node_t *src,
		      dt_node_t *size, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dtrace_hdl_t	*dtp = yypcb->pcb_hdl;
	int		maxsize = dtp->dt_options[DTRACEOPT_SCRATCHSIZE] - 8;
	uint_t		lbl_badsize = dt_irlist_label(dlp);
	uint_t		lbl_ok = dt_irlist_label(dlp);
	int		is_bcopy = dnp->dn_ident->di_id == DIF_SUBR_BCOPY;

	TRACE_REGSET("      subr-bcopy-impl:Begin");

	/* Evaluate the arguments */
	if (is_bcopy) {
		dt_cg_node(src, dlp, drp);
		dt_cg_node(dst, dlp, drp);
		dt_cg_node(size, dlp, drp);
	} else {
		dt_cg_node(src, dlp, drp);
		dt_cg_node(size, dlp, drp);
		dt_cg_node(dst, dlp, drp);
	}

	/* Validate the size for the copy operation. */
	emit(dlp,  BPF_BRANCH_IMM(BPF_JSLT, size->dn_reg, 0, lbl_badsize));
	emit(dlp,  BPF_BRANCH_IMM(BPF_JGT, size->dn_reg, maxsize, lbl_badsize));

	/* Validate the source pointer. */
	dt_cg_check_ptr_arg(dlp, drp, src, size);

	/* Validate the destination pointer. */
	if (!(dst->dn_flags & DT_NF_ALLOCA))
		dnerror(dst, D_PROTO_ARG,
			"%s( ) argument #%d is incompatible with prototype:\n"
			"\tprototype: alloca pointer\n"
			"\t argument: non-alloca pointer\n",
			is_bcopy ? "bcopy" : "copyinto", is_bcopy ? 2 : 3);
	dt_cg_check_ptr_arg(dlp, drp, dst, size);

	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	emit(dlp,  BPF_MOV_REG(BPF_REG_1, dst->dn_reg));
	emit(dlp,  BPF_MOV_REG(BPF_REG_2, size->dn_reg));
	emit(dlp,  BPF_MOV_REG(BPF_REG_3, src->dn_reg));
	dt_regset_xalloc(drp, BPF_REG_0);
	emit(dlp,  BPF_CALL_HELPER(dtp->dt_bpfhelper[
					is_bcopy ? BPF_FUNC_probe_read_kernel
						 : BPF_FUNC_probe_read_user]));

	/*
	 * At this point the dst is validated, so any problem must be with
	 * the src address.
	 */
	emit(dlp,  BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_0, 0, lbl_ok));
	dt_regset_free(drp, BPF_REG_0);
	dt_regset_free_args(drp);
	dt_cg_probe_error(yypcb, DTRACEFLT_BADADDR, DT_ISREG, src->dn_reg);
	emitl(dlp, lbl_badsize,
		   BPF_NOP());
	dt_cg_probe_error(yypcb, DTRACEFLT_BADSIZE, DT_ISREG, size->dn_reg);
	emitl(dlp, lbl_ok,
		   BPF_NOP());

	dt_regset_free(drp, src->dn_reg);
	dt_regset_free(drp, dst->dn_reg);
	dt_regset_free(drp, size->dn_reg);

	TRACE_REGSET("      subr-bcopy-impl:End  ");
}

static void
dt_cg_subr_bcopy(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_node_t	*src = dnp->dn_args;
	dt_node_t	*dst = src->dn_list;
	dt_node_t	*size = dst->dn_list;

	TRACE_REGSET("    subr-bcopy:Begin");

	dt_cg_subr_bcopy_impl(dnp, dst, src, size, dlp, drp);

	TRACE_REGSET("    subr-bcopy:End  ");
}

static void
dt_cg_subr_copyin(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dtrace_hdl_t	*dtp = yypcb->pcb_hdl;
	dt_node_t	*src = dnp->dn_args;
	dt_node_t	*size = src->dn_list;
	int		maxsize = dtp->dt_options[DTRACEOPT_SCRATCHSIZE] - 8;
	uint_t		lbl_ok = dt_irlist_label(dlp);
	uint_t		lbl_badsize = dt_irlist_label(dlp);

	TRACE_REGSET("    subr-copyin:Begin");

	/* Validate the size for the copy operation. */
	dt_cg_node(size, dlp, drp);

	emit(dlp,  BPF_BRANCH_IMM(BPF_JSLT, size->dn_reg, 0, lbl_badsize));
	emit(dlp,  BPF_BRANCH_IMM(BPF_JGT, size->dn_reg, maxsize, lbl_badsize));

	if ((dnp->dn_reg = dt_regset_alloc(drp)) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	/* Allocate scratch space. */
	dt_cg_subr_alloca_impl(dnp, size, dlp, drp);
	/* Push the native alloca value to be used as return value. */
	dt_cg_push_stack(dnp->dn_reg, dlp, drp);
	/* Turn it into a proper alloca pointer. */
	dt_cg_alloca_ptr(dlp, drp, dnp->dn_reg, dnp->dn_reg);

	dt_cg_node(src, dlp, drp);

	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	emit(dlp, BPF_MOV_REG(BPF_REG_1, dnp->dn_reg));
	emit(dlp, BPF_MOV_REG(BPF_REG_2, size->dn_reg));
	emit(dlp, BPF_MOV_REG(BPF_REG_3, src->dn_reg));
	dt_regset_xalloc(drp, BPF_REG_0);
	emit(dlp, BPF_CALL_HELPER(dtp->dt_bpfhelper[BPF_FUNC_probe_read_user]));
	dt_regset_free_args(drp);
	emit(dlp,  BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_0, 0, lbl_ok));
	dt_regset_free(drp, BPF_REG_0);
	dt_cg_probe_error(yypcb, DTRACEFLT_BADADDR, DT_ISREG, src->dn_reg);
	dt_regset_free(drp, src->dn_reg);
	emitl(dlp, lbl_badsize,
		   BPF_NOP());
	dt_cg_probe_error(yypcb, DTRACEFLT_BADSIZE, DT_ISREG, size->dn_reg);
	dt_regset_free(drp, size->dn_reg);
	emitl(dlp, lbl_ok,
		   BPF_NOP());
	/* Pop the native alloca value as our value. */
	dt_cg_pop_stack(dnp->dn_reg, dlp, drp);

	TRACE_REGSET("    subr-copyin:End  ");
}

static void
dt_cg_subr_copyinstr(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dtrace_hdl_t	*dtp = yypcb->pcb_hdl;
	dt_node_t	*src = dnp->dn_args;
	dt_node_t	*size = src->dn_list;
	int		maxsize = dtp->dt_options[DTRACEOPT_STRSIZE];
	uint_t		lbl_ok = dt_irlist_label(dlp);
	uint_t		lbl_oksize = dt_irlist_label(dlp);

	TRACE_REGSET("    subr-copyinstr:Begin");

	/* Validate the size for the copy operation (if provided). */
	if (size == NULL) {
		size = dt_node_int(maxsize);
		src->dn_list = size;
	}

	dt_cg_node(size, dlp, drp);

	emit(dlp,  BPF_BRANCH_IMM(BPF_JLE, size->dn_reg, maxsize, lbl_oksize));
	emit(dlp,  BPF_MOV_IMM(size->dn_reg, maxsize));
	emitl(dlp, lbl_oksize,
		   BPF_NOP());

	/* Allocate a temporary string for the destination (return value). */
	dnp->dn_reg = dt_regset_alloc(drp);
	if (dnp->dn_reg == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	dt_cg_tstring_alloc(yypcb, dnp);
	dt_cg_access_dctx(dnp->dn_reg, dlp, drp, DCTX_MEM);
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, dnp->dn_reg, dnp->dn_tstring->dn_value));

	dt_cg_node(src, dlp, drp);

	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	emit(dlp, BPF_MOV_REG(BPF_REG_1, dnp->dn_reg));
	emit(dlp, BPF_MOV_REG(BPF_REG_2, size->dn_reg));
	emit(dlp, BPF_MOV_REG(BPF_REG_3, src->dn_reg));
	dt_regset_xalloc(drp, BPF_REG_0);
	emit(dlp, BPF_CALL_HELPER(dtp->dt_bpfhelper[BPF_FUNC_probe_read_user_str]));
	dt_regset_free_args(drp);
	emit(dlp,  BPF_BRANCH_IMM(BPF_JSGT, BPF_REG_0, 0, lbl_ok));
	dt_cg_probe_error(yypcb, DTRACEFLT_BADADDR, DT_ISREG, src->dn_reg);
	emitl(dlp, lbl_ok,
		   BPF_NOP());
	dt_regset_free(drp, BPF_REG_0);

	dt_regset_free(drp, src->dn_reg);
	dt_regset_free(drp, size->dn_reg);

	TRACE_REGSET("    subr-copyinstr:End  ");
}

static void
dt_cg_subr_copyinto(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_node_t	*src = dnp->dn_args;
	dt_node_t	*size = src->dn_list;
	dt_node_t	*dst = size->dn_list;

	TRACE_REGSET("    subr-copyinto:Begin");

	dt_cg_subr_bcopy_impl(dnp, dst, src, size, dlp, drp);

	TRACE_REGSET("    subr-copyinto:End  ");
}

static void
dt_cg_subr_copyout(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_node_t	*src = dnp->dn_args;
	dt_node_t	*dst = src->dn_list;
	dt_node_t	*size = dst->dn_list;
	uint_t		lbl_ok = dt_irlist_label(dlp);

	dt_cg_clsflags(yypcb, DTRACEACT_PROC_DESTRUCTIVE, dnp);

	TRACE_REGSET("    subr-copyout:Begin");

	dt_cg_node(src, dlp, drp);
	dt_cg_node(dst, dlp, drp);
	dt_cg_node(size, dlp, drp);

	/* Validate the pointers. */
	dt_cg_check_ptr_arg(dlp, drp, src, size);
	dt_cg_check_notnull(dlp, drp, dst->dn_reg);

	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	emit(dlp,  BPF_MOV_REG(BPF_REG_1, dst->dn_reg));
	emit(dlp,  BPF_MOV_REG(BPF_REG_2, src->dn_reg));
	emit(dlp,  BPF_MOV_REG(BPF_REG_3, size->dn_reg));
	dt_regset_xalloc(drp, BPF_REG_0);
	emit(dlp,  BPF_CALL_HELPER(BPF_FUNC_probe_write_user));
	dt_regset_free_args(drp);

	/*
	 * At this point the src is validated, so any problem must be with
	 * the dst address.
	 */
	emit(dlp,  BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_0, 0, lbl_ok));
	dt_regset_free(drp, BPF_REG_0);
	dt_cg_probe_error(yypcb, DTRACEFLT_BADADDR, DT_ISREG, dst->dn_reg);
	emitl(dlp, lbl_ok,
		   BPF_NOP());

	dt_regset_free(drp, src->dn_reg);
	dt_regset_free(drp, dst->dn_reg);
	dt_regset_free(drp, size->dn_reg);

	TRACE_REGSET("    subr-copyout:End  ");
}

static void
dt_cg_subr_copyoutstr(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_node_t	*src = dnp->dn_args;
	dt_node_t	*dst = src->dn_list;
	dt_node_t	*size = dst->dn_list;
	size_t		strsize = yypcb->pcb_hdl->dt_options[DTRACEOPT_STRSIZE];
	uint64_t	off;
	uint_t		L1 = dt_irlist_label(dlp);
	uint_t		L2 = dt_irlist_label(dlp);
	uint_t		L3 = dt_irlist_label(dlp);

	dt_cg_clsflags(yypcb, DTRACEACT_PROC_DESTRUCTIVE, dnp);

	TRACE_REGSET("    subr-copyoutstr:Begin");

	dt_cg_node(src, dlp, drp);
	dt_cg_node(dst, dlp, drp);
	dt_cg_node(size, dlp, drp);

	/* if (size > strsize) size = strsize */
	emit(dlp,  BPF_BRANCH_IMM(BPF_JLE, size->dn_reg, strsize, L1));
	emit(dlp,  BPF_MOV_IMM(size->dn_reg, strsize));
	emitl(dlp, L1,
		   BPF_NOP());

	/* validate the pointers */
	dt_cg_check_ptr_arg(dlp, drp, src, size);
	dt_cg_check_notnull(dlp, drp, dst->dn_reg);

	/* copy into a temporary string to determine the string length */
	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	emit(dlp, BPF_MOV_REG(BPF_REG_2, size->dn_reg));
	dt_regset_free(drp, size->dn_reg);
	emit(dlp, BPF_MOV_REG(BPF_REG_3, src->dn_reg));
	off = dt_cg_tstring_xalloc(yypcb);
	dt_cg_access_dctx(BPF_REG_1, dlp, drp, DCTX_MEM);
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, off));
	dt_regset_xalloc(drp, BPF_REG_0);
	emit(dlp, BPF_CALL_HELPER(BPF_FUNC_probe_read_str));
	dt_regset_free_args(drp);
	dt_cg_tstring_xfree(yypcb, off);

	/* check that we could read the string */
	emit(dlp,  BPF_BRANCH_IMM(BPF_JSGT, BPF_REG_0, 0, L2));
	dt_cg_probe_error(yypcb, DTRACEFLT_BADADDR, DT_ISREG, src->dn_reg);  // FIXME: is this right?
	emitl(dlp, L2,
		   BPF_NOP());

	/* copy with the measured size */
	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	emit(dlp,  BPF_MOV_REG(BPF_REG_1, dst->dn_reg));
	emit(dlp,  BPF_MOV_REG(BPF_REG_2, src->dn_reg));
	emit(dlp,  BPF_MOV_REG(BPF_REG_3, BPF_REG_0));
	emit(dlp,  BPF_CALL_HELPER(BPF_FUNC_probe_write_user));
	dt_regset_free_args(drp);

	/* since src was validated, any problem must be with dst */
	emit(dlp,  BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_0, 0, L3));
	dt_regset_free(drp, BPF_REG_0);
	dt_cg_probe_error(yypcb, DTRACEFLT_BADADDR, DT_ISREG, dst->dn_reg);
	emitl(dlp, L3,
		   BPF_NOP());

	dt_regset_free(drp, src->dn_reg);
	dt_regset_free(drp, dst->dn_reg);

	TRACE_REGSET("    subr-copyoutstr:End  ");
}

static void
dt_cg_subr_strchr(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_ident_t	*idp;
	dt_node_t	*str = dnp->dn_args;
	dt_node_t	*chr = str->dn_list;
	uint64_t	off;
	uint_t		Lfound = dt_irlist_label(dlp);

	TRACE_REGSET("    subr-strchr:Begin");
	dt_cg_node(str, dlp, drp);
	dt_cg_check_ptr_arg(dlp, drp, str, NULL);
	dt_cg_node(chr, dlp, drp);

	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	emit(dlp, BPF_MOV_REG(BPF_REG_1, str->dn_reg));
	dt_regset_free(drp, str->dn_reg);
	emit(dlp, BPF_MOV_REG(BPF_REG_2, chr->dn_reg));
	dt_regset_free(drp, chr->dn_reg);

	/*
	 * The result needs to be a temporary string, so we request one.
	 */
	dnp->dn_reg = dt_regset_alloc(drp);
	if (dnp->dn_reg == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	dt_cg_tstring_alloc(yypcb, dnp);

	dt_cg_access_dctx(dnp->dn_reg, dlp, drp, DCTX_MEM);
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, dnp->dn_reg, dnp->dn_tstring->dn_value));
	emit(dlp,  BPF_MOV_REG(BPF_REG_3, dnp->dn_reg));

	off = dt_cg_tstring_xalloc(yypcb);
	dt_cg_access_dctx(BPF_REG_4, dlp, drp, DCTX_MEM);
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_4, off));

	idp = dt_dlib_get_func(yypcb->pcb_hdl, "dt_strchr");
	assert(idp != NULL);
	dt_regset_xalloc(drp, BPF_REG_0);
	emite(dlp,  BPF_CALL_FUNC(idp->di_id), idp);
	dt_regset_free_args(drp);
	dt_cg_tstring_xfree(yypcb, off);
	dt_cg_tstring_free(yypcb, str);

	emit (dlp, BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_0, 0, Lfound));
	emit (dlp, BPF_MOV_IMM(dnp->dn_reg, 0));
	emitl(dlp, Lfound,
		   BPF_NOP());
	dt_regset_free(drp, BPF_REG_0);

	TRACE_REGSET("    subr-strchr:End  ");
}

static void
dt_cg_subr_strrchr(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_ident_t	*idp;
	dt_node_t	*str = dnp->dn_args;
	dt_node_t	*chr = str->dn_list;
	uint_t		Lfound = dt_irlist_label(dlp);

	TRACE_REGSET("    subr-strrchr:Begin");
	dt_cg_node(str, dlp, drp);
	dt_cg_check_ptr_arg(dlp, drp, str, NULL);
	dt_cg_node(chr, dlp, drp);

	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	emit(dlp, BPF_MOV_REG(BPF_REG_1, str->dn_reg));
	dt_regset_free(drp, str->dn_reg);
	emit(dlp, BPF_MOV_REG(BPF_REG_2, chr->dn_reg));
	dt_regset_free(drp, chr->dn_reg);

	/*
	 * The result needs to be a temporary string, so we request one.
	 */
	dnp->dn_reg = dt_regset_alloc(drp);
	if (dnp->dn_reg == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	dt_cg_tstring_alloc(yypcb, dnp);

	dt_cg_access_dctx(dnp->dn_reg, dlp, drp, DCTX_MEM);
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, dnp->dn_reg, dnp->dn_tstring->dn_value));
	emit(dlp,  BPF_MOV_REG(BPF_REG_3, dnp->dn_reg));

	idp = dt_dlib_get_func(yypcb->pcb_hdl, "dt_strrchr");
	assert(idp != NULL);
	dt_regset_xalloc(drp, BPF_REG_0);
	emite(dlp,  BPF_CALL_FUNC(idp->di_id), idp);
	dt_regset_free_args(drp);
	dt_cg_tstring_free(yypcb, str);

	emit (dlp, BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_0, 0, Lfound));
	emit (dlp, BPF_MOV_IMM(dnp->dn_reg, 0));
	emitl(dlp, Lfound,
		   BPF_NOP());
	dt_regset_free(drp, BPF_REG_0);

	TRACE_REGSET("    subr-strrchr:End  ");
}

static void
dt_cg_subr_strlen(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_ident_t	*idp = dt_dlib_get_func(yypcb->pcb_hdl, "dt_strlen");
	dt_node_t	*str = dnp->dn_args;

	assert(idp != NULL);

	TRACE_REGSET("    subr-strlen:Begin");

	dt_cg_node(str, dlp, drp);
	dt_cg_check_ptr_arg(dlp, drp, str, NULL);

	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	dt_cg_access_dctx(BPF_REG_1, dlp, drp, -1);
	emit(dlp, BPF_MOV_REG(BPF_REG_2, str->dn_reg));
	dt_regset_free(drp, str->dn_reg);
	dt_cg_tstring_free(yypcb, str);
	dt_regset_xalloc(drp, BPF_REG_0);
	emite(dlp,BPF_CALL_FUNC(idp->di_id), idp);

	dt_regset_free_args(drp);
	dnp->dn_reg = dt_regset_alloc(drp);
	if (dnp->dn_reg == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	emit(dlp, BPF_MOV_REG(dnp->dn_reg, BPF_REG_0));
	dt_regset_free(drp, BPF_REG_0);

	TRACE_REGSET("    subr-strlen:End  ");
}

static void
dt_cg_subr_strjoin(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_node_t	*s1 = dnp->dn_args;
	dt_node_t	*s2 = s1->dn_list;
	dt_ident_t	*idp = dt_dlib_get_func(yypcb->pcb_hdl, "dt_strjoin");

	assert(idp != NULL);

	TRACE_REGSET("    subr-strjoin:Begin");

	dt_cg_node(s1, dlp, drp);
	dt_cg_check_ptr_arg(dlp, drp, s1, NULL);
	dt_cg_node(s2, dlp, drp);
	dt_cg_check_ptr_arg(dlp, drp, s2, NULL);

	/*
	 * The result needs to be a temporary string, so we request one.
	 */
	dnp->dn_reg = dt_regset_alloc(drp);
	if (dnp->dn_reg == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	dt_cg_tstring_alloc(yypcb, dnp);

	dt_cg_access_dctx(dnp->dn_reg, dlp, drp, DCTX_MEM);
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, dnp->dn_reg, dnp->dn_tstring->dn_value));

	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	emit(dlp,  BPF_MOV_REG(BPF_REG_1, dnp->dn_reg));
	emit(dlp,  BPF_MOV_REG(BPF_REG_2, s1->dn_reg));
	dt_regset_free(drp, s1->dn_reg);
	dt_cg_tstring_free(yypcb, s1);
	emit(dlp,  BPF_MOV_REG(BPF_REG_3, s2->dn_reg));
	dt_regset_free(drp, s2->dn_reg);
	dt_cg_tstring_free(yypcb, s2);
	dt_regset_xalloc(drp, BPF_REG_0);
	emite(dlp, BPF_CALL_FUNC(idp->di_id), idp);
	dt_regset_free_args(drp);
	dt_regset_free(drp, BPF_REG_0);

	TRACE_REGSET("    subr-strjoin:End  ");
}

static void
dt_cg_subr_strstr(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_node_t	*s = dnp->dn_args;
	dt_node_t	*t = s->dn_list;
	dt_ident_t	*idp;
	uint64_t	off1, off2;
	uint_t		Ldone = dt_irlist_label(dlp);

	TRACE_REGSET("    subr-strstr:Begin");

	/* get args */
	dt_cg_node(s, dlp, drp);
	dt_cg_check_ptr_arg(dlp, drp, s, NULL);
	dt_cg_node(t, dlp, drp);
	dt_cg_check_ptr_arg(dlp, drp, t, NULL);

	/* call dt_index() call */
	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	emit(dlp,  BPF_MOV_REG(BPF_REG_1, s->dn_reg));
	emit(dlp,  BPF_MOV_REG(BPF_REG_2, t->dn_reg));
	dt_regset_free(drp, t->dn_reg);
	dt_cg_tstring_free(yypcb, t);
	emit(dlp,  BPF_MOV_IMM(BPF_REG_3, 0));

	off1 = dt_cg_tstring_xalloc(yypcb);
	off2 = dt_cg_tstring_xalloc(yypcb);

	dt_cg_access_dctx(BPF_REG_4, dlp, drp, DCTX_MEM);
	emit(dlp,  BPF_MOV_REG(BPF_REG_5, BPF_REG_4));
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_4, off1));
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_5, off2));

	idp = dt_dlib_get_func(yypcb->pcb_hdl, "dt_index");
	assert(idp != NULL);
	dt_regset_xalloc(drp, BPF_REG_0);
	emite(dlp, BPF_CALL_FUNC(idp->di_id), idp);
	dt_regset_free_args(drp);

	dt_cg_tstring_xfree(yypcb, off1);
	dt_cg_tstring_xfree(yypcb, off2);

	/*
	 * Allocate the result register and associate it with a temporary
	 * string slot.
	 */
	dnp->dn_reg = dt_regset_alloc(drp);
	if (dnp->dn_reg == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	dt_cg_tstring_alloc(yypcb, dnp);

	/* account for not finding the substring */

	emit(dlp,  BPF_MOV_IMM(dnp->dn_reg, 0));
	emit(dlp,  BPF_BRANCH_IMM(BPF_JSLT, BPF_REG_0, 0, Ldone));

	dt_cg_access_dctx(dnp->dn_reg, dlp, drp, DCTX_MEM);
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, dnp->dn_reg, dnp->dn_tstring->dn_value));

	/* call dt_substr() using the index we already found */

	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	emit(dlp, BPF_MOV_REG(BPF_REG_1, dnp->dn_reg));
	emit(dlp, BPF_MOV_REG(BPF_REG_2, s->dn_reg));
	dt_regset_free(drp, s->dn_reg);
	dt_cg_tstring_free(yypcb, s);
	emit(dlp, BPF_MOV_REG(BPF_REG_3, BPF_REG_0));
	emit(dlp, BPF_MOV_IMM(BPF_REG_5, 2));
	idp = dt_dlib_get_func(yypcb->pcb_hdl, "dt_substr");
	assert(idp != NULL);
	emite(dlp,  BPF_CALL_FUNC(idp->di_id), idp);
	dt_regset_free_args(drp);

	emitl(dlp, Ldone,
		   BPF_NOP());

	dt_regset_free(drp, BPF_REG_0);

	TRACE_REGSET("    subr-strstr:End  ");
}

static void
dt_cg_subr_strtok(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dtrace_hdl_t	*dtp = yypcb->pcb_hdl;
	dt_node_t	*str = dnp->dn_args;
	dt_node_t	*del = str->dn_list;
	dt_ident_t	*idp = dt_dlib_get_func(yypcb->pcb_hdl, "dt_strtok");
	uint64_t	off;
	int		reg;

	assert(idp != NULL);

	TRACE_REGSET("    subr-strtok:Begin");

	reg = dt_regset_alloc(drp);
	if (reg == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	dt_cg_access_dctx(reg, dlp, drp, DCTX_MEM);

	/*
	 * Start with a static check for a NULL string.
	 * That is, strtok(NULL, foo) has a special meaning:
	 * continue parsing the previous string.  In contrast,
	 * strtok(str, foo) (even if str==NULL) means to parse str.
	 */
	if (str->dn_op != DT_TOK_INT || str->dn_value != 0) {
		/* string is present:  copy it to internal state */
		dt_cg_node(str, dlp, drp);
		dt_cg_check_ptr_arg(dlp, drp, str, NULL);

		if (dt_regset_xalloc_args(drp) == -1)
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

		/* the 8-byte prefix is the offset, which we initialize to 0 */
		emit(dlp,  BPF_MOV_REG(BPF_REG_1, reg));
		emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, DMEM_STRTOK(dtp)));
		emit(dlp,  BPF_STORE_IMM(BPF_DW, BPF_REG_1, 0, 0));
		emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, 8));
		emit(dlp,  BPF_MOV_IMM(BPF_REG_2, dtp->dt_options[DTRACEOPT_STRSIZE] + 1));
		emit(dlp,  BPF_MOV_REG(BPF_REG_3, str->dn_reg));
		dt_regset_free(drp, str->dn_reg);
		if (str->dn_tstring)
			dt_cg_tstring_free(yypcb, str);
		dt_regset_xalloc(drp, BPF_REG_0);
		emit(dlp,  BPF_CALL_HELPER(BPF_FUNC_probe_read_str));
		dt_regset_free(drp, BPF_REG_0);
		dt_regset_free_args(drp);
	} else {
		/* NULL string:  error if internal state is uninitialized */
		emit(dlp,  BPF_MOV_REG(BPF_REG_0, reg));
		emit(dlp,  BPF_LOAD(BPF_DW, BPF_REG_0, BPF_REG_0, DMEM_STRTOK(dtp)));
		emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_0, 1));
		dt_cg_check_notnull(dlp, drp, BPF_REG_0);
	}

	/* get delimiters */
	dt_cg_node(del, dlp, drp);
	dt_cg_check_ptr_arg(dlp, drp, del, NULL);

	/* allocate temporary string for result */
	dnp->dn_reg = dt_regset_alloc(drp);
	if (dnp->dn_reg == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	dt_cg_tstring_alloc(yypcb, dnp);
	emit(dlp,  BPF_MOV_REG(dnp->dn_reg, reg));
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, dnp->dn_reg, dnp->dn_tstring->dn_value));

	/* allocate temporary string for internal purposes */
	off = dt_cg_tstring_xalloc(yypcb);

	/* function call */
	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	emit(dlp,  BPF_MOV_REG(BPF_REG_1, dnp->dn_reg));
	emit(dlp,  BPF_MOV_REG(BPF_REG_2, reg));
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_2, DMEM_STRTOK(dtp)));
	emit(dlp,  BPF_MOV_REG(BPF_REG_3, del->dn_reg));
	dt_regset_free(drp, del->dn_reg);
	if (del->dn_tstring)
		dt_cg_tstring_free(yypcb, del);

	emit(dlp,  BPF_MOV_REG(BPF_REG_4, reg));
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_4, off));

	dt_regset_xalloc(drp, BPF_REG_0);
	emite(dlp, BPF_CALL_FUNC(idp->di_id), idp);
	dt_regset_free_args(drp);
	dt_cg_tstring_xfree(yypcb, off);
	emit(dlp,  BPF_MOV_REG(dnp->dn_reg, BPF_REG_0));
	dt_regset_free(drp, BPF_REG_0);

	dt_regset_free(drp, reg);

	TRACE_REGSET("    subr-strtok:End  ");
}

static void
dt_cg_subr_substr(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_node_t	*str = dnp->dn_args;
	dt_node_t	*idx = str->dn_list;
	dt_node_t	*cnt = idx->dn_list;
	dt_ident_t	*idp;

	TRACE_REGSET("    subr-substr:Begin");

	dt_cg_node(str, dlp, drp);
	dt_cg_check_ptr_arg(dlp, drp, str, NULL);
	dt_cg_node(idx, dlp, drp);
	if (cnt != NULL)
		dt_cg_node(cnt, dlp, drp);

	/*
	 * Allocate a temporary string slot for the result.
	 */
	dt_cg_tstring_alloc(yypcb, dnp);

	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	dt_cg_access_dctx(BPF_REG_1, dlp, drp, DCTX_MEM);
	emit(dlp,  BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, dnp->dn_tstring->dn_value));
	emit(dlp,  BPF_MOV_REG(BPF_REG_2, str->dn_reg));
	dt_regset_free(drp, str->dn_reg);
	dt_cg_tstring_free(yypcb, str);
	emit(dlp,  BPF_MOV_REG(BPF_REG_3, idx->dn_reg));
	dt_regset_free(drp, idx->dn_reg);
	if (cnt != NULL) {
		emit(dlp, BPF_MOV_REG(BPF_REG_4, cnt->dn_reg));
		dt_regset_free(drp, cnt->dn_reg);
		emit(dlp, BPF_MOV_IMM(BPF_REG_5, 3));
	} else {
		emit(dlp, BPF_MOV_IMM(BPF_REG_4, 0));
		emit(dlp, BPF_MOV_IMM(BPF_REG_5, 2));
	}
	idp = dt_dlib_get_func(yypcb->pcb_hdl, "dt_substr");
	assert(idp != NULL);
	dt_regset_xalloc(drp, BPF_REG_0);
	emite(dlp,  BPF_CALL_FUNC(idp->di_id), idp);

	/*
	 * Allocate the result register, and assign the result to it..
	 */
	dnp->dn_reg = dt_regset_alloc(drp);
	if (dnp->dn_reg == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	emit(dlp, BPF_MOV_REG(dnp->dn_reg, BPF_REG_0));
	dt_regset_free(drp, BPF_REG_0);
	dt_regset_free_args(drp);

	TRACE_REGSET("    subr-substr:End  ");
}

static void
dt_cg_subr_htons(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_cg_node(dnp->dn_args, dlp, drp);
	dnp->dn_reg = dnp->dn_args->dn_reg;
	emit(dlp, BPF_END_REG(BPF_H, dnp->dn_reg, BPF_TO_BE));
}

static void
dt_cg_subr_htonl(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_cg_node(dnp->dn_args, dlp, drp);
	dnp->dn_reg = dnp->dn_args->dn_reg;
	emit(dlp, BPF_END_REG(BPF_W, dnp->dn_reg, BPF_TO_BE));
}

static void
dt_cg_subr_htonll(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_cg_node(dnp->dn_args, dlp, drp);
	dnp->dn_reg = dnp->dn_args->dn_reg;
	emit(dlp, BPF_END_REG(BPF_DW, dnp->dn_reg, BPF_TO_BE));
}

static void
dt_cg_subr_inet_ntoa(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_cg_subr_arg_to_tstring(dnp, dlp, drp, "dt_inet_ntoa", 0, DT_IGNOR, 0,
				  DT_IGNOR, 0);
}

static void
dt_cg_subr_inet_ntoa6(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dtrace_hdl_t	*dtp = yypcb->pcb_hdl;
	dt_node_t	*addr = dnp->dn_args;
	dt_node_t	*strict = addr->dn_list;
	ssize_t		tbloff;
	/*
	 * This table encodes the start and length of the longest run of 0 bits
	 * in the binary representation of the table index, if any such run of
	 * length >= 2 exists.  Each value pair (start, length) is stored as a
	 * single byte: (start << 4) | length.  If no rush run exists, the
	 * value 0x70 is used.
	 *
	 * See bpf/inet_ntoa6.S for details on how this table is used.
	 */
	static const char	tbl[] = {
			0x08, 0x07, 0x06, 0x06, 0x05, 0x05, 0x05, 0x05,
			0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
			0x44, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
			0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
			0x35, 0x34, 0x33, 0x33, 0x02, 0x02, 0x02, 0x02,
			0x53, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
			0x44, 0x43, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
			0x53, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
			0x26, 0x25, 0x24, 0x24, 0x23, 0x23, 0x23, 0x23,
			0x53, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
			0x44, 0x43, 0x42, 0x42, 0x62, 0x70, 0x70, 0x70,
			0x53, 0x52, 0x70, 0x70, 0x62, 0x70, 0x70, 0x70,
			0x35, 0x34, 0x33, 0x33, 0x32, 0x32, 0x32, 0x32,
			0x53, 0x52, 0x70, 0x70, 0x62, 0x70, 0x70, 0x70,
			0x44, 0x43, 0x42, 0x42, 0x62, 0x70, 0x70, 0x70,
			0x53, 0x52, 0x70, 0x70, 0x62, 0x70, 0x70, 0x70,
			0x17, 0x16, 0x15, 0x15, 0x14, 0x14, 0x14, 0x14,
			0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13,
			0x44, 0x43, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,
			0x53, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,
			0x35, 0x34, 0x33, 0x33, 0x32, 0x32, 0x32, 0x32,
			0x53, 0x52, 0x70, 0x70, 0x62, 0x70, 0x70, 0x70,
			0x44, 0x43, 0x42, 0x42, 0x62, 0x70, 0x70, 0x70,
			0x53, 0x52, 0x70, 0x70, 0x62, 0x70, 0x70, 0x70,
			0x26, 0x25, 0x24, 0x24, 0x23, 0x23, 0x23, 0x23,
			0x53, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
			0x44, 0x43, 0x42, 0x42, 0x62, 0x70, 0x70, 0x70,
			0x53, 0x52, 0x70, 0x70, 0x62, 0x70, 0x70, 0x70,
			0x35, 0x34, 0x33, 0x33, 0x32, 0x32, 0x32, 0x32,
			0x53, 0x52, 0x70, 0x70, 0x62, 0x70, 0x70, 0x70,
			0x44, 0x43, 0x42, 0x42, 0x62, 0x70, 0x70, 0x70,
			0x53, 0x52, 0x70, 0x70, 0x62, 0x70, 0x70, 0x70,
	};

	if (strict != NULL && strict->dn_kind != DT_NODE_INT)
		dnerror(strict, D_PROTO_ARG, "inet_ntoa6( ) argument #2 "
			"must be an integer constant\n");

	tbloff = dt_rodata_insert(dtp->dt_rodata, tbl, 256);
	if (tbloff == -1L)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOMEM);
	if (tbloff > DIF_STROFF_MAX)
		longjmp(yypcb->pcb_jmpbuf, EDT_STR2BIG);

	dt_cg_subr_arg_to_tstring(dnp, dlp, drp, "dt_inet_ntoa6", 0,
				  DT_ISIMM, tbloff,
				  DT_ISIMM, strict && strict->dn_value ? 1 : 0);
}

static void
dt_cg_subr_inet_ntop(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_node_t	*af = dnp->dn_args;
	dt_node_t	*addr = af->dn_list;
	dt_node_t	*tnp, *cnp, *lnp, *rnp, *anp, *xnp;
	dt_idsig_t	*isp;
	dt_decl_t	*ddp;

	/*
	 * Implement this subroutine as a ternary expression:
	 *
	 *	af == AF_INET6 ? inet_ntoa6((typ1)addr) : inet_ntoa((typ2)addr)
	 *
	 * where typ1 is the datatype of the argument of inet_ntoa6()
	 *	 typ2 is the datatype of the argument of inet_ntoa()
	 */
	cnp = dt_node_op2(DT_TOK_EQU, af, dt_node_int(AF_INET6));

	/* Create a node for the function call: inet_ntoa6(addr) */
	lnp = dt_node_func(dt_node_ident(strdup("inet_ntoa6")), addr);

	/* Get the datatype of the argument of inet_ntoa6(). */
	assert(lnp->dn_ident && lnp->dn_ident->di_data);
	isp = lnp->dn_ident->di_data;
	assert(isp->dis_args);
	anp = &isp->dis_args[0];
	ddp = dt_decl_alloc(ctf_type_kind(anp->dn_ctfp, anp->dn_type), NULL);
	ddp->dd_ctfp = anp->dn_ctfp;
	ddp->dd_type = anp->dn_type;
	xnp = dt_node_type(ddp);		/* frees ddp */
	xnp = dt_node_op2(DT_TOK_LPAR, xnp, addr); /* (type)addr */
	xnp->dn_list = dt_node_int(1);

	lnp->dn_args = xnp;			/* inet_ntoa6((type)addr) */

	/* Create a node for the function call: inet_ntoa(addr) */
	rnp = dt_node_func(dt_node_ident(strdup("inet_ntoa")), addr);

	/* Get the datatype of the argument of inet_ntoa(). */
	assert(rnp->dn_ident && rnp->dn_ident->di_data);
	isp = rnp->dn_ident->di_data;
	assert(isp->dis_args);
	anp = &isp->dis_args[0];
	ddp = dt_decl_alloc(ctf_type_kind(anp->dn_ctfp, anp->dn_type), NULL);
	ddp->dd_ctfp = anp->dn_ctfp;
	ddp->dd_type = anp->dn_type;
	xnp = dt_node_type(ddp);		/* frees ddp */
	xnp = dt_node_op2(DT_TOK_LPAR, xnp, addr); /* (type)addr */
	rnp->dn_args = xnp;			/* inet_ntoa((type)addr) */

	tnp = dt_node_op3(cnp, lnp, rnp);	/* cnp ? lnp : rnp */

	/* Finalize the node tree and generate code. */
	dt_node_cook(tnp, 0);
	dt_cg_node(tnp, dlp, drp);

	/*
	 * Copy the result into dnp (register value and tstring).  We need to
	 * clear the tstring from tnp once we move it to dnp.
	 */
	dnp->dn_reg = tnp->dn_reg;
	dnp->dn_tstring = tnp->dn_tstring;
	tnp->dn_tstring = NULL;
}

static void
dt_cg_subr_d_path(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_node_t	*arg = dnp->dn_args;

	dt_cg_node(arg, dlp, drp);
	dt_cg_check_ptr_arg(dlp, drp, arg, NULL);

	/* Allocate a temporary string for the result. */
	dnp->dn_reg = dt_regset_alloc(drp);
	if (dnp->dn_reg == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);
	dt_cg_tstring_alloc(yypcb, dnp);
	dt_cg_access_dctx(dnp->dn_reg, dlp, drp, DCTX_MEM);
	emit(dlp, BPF_ALU64_IMM(BPF_ADD, dnp->dn_reg, dnp->dn_tstring->dn_value));

	/*
	 * We do not have a real implementation yet, so throw away the result
	 * of evaluating the argument, and return a default value "<unknown>"
	 * in a temporary string.
	 *
	 * <unknown> is 3c 75 6e 6b 6e 6f 77 6e 3e 00
	 */
	dt_regset_free(drp, arg->dn_reg);
	dt_cg_tstring_free(yypcb, arg);

	emit(dlp, BPF_STORE_IMM(BPF_W, dnp->dn_reg, 0, 0x6b6e753c));
	emit(dlp, BPF_STORE_IMM(BPF_W, dnp->dn_reg, 4, 0x6e776f6e));
	emit(dlp, BPF_STORE_IMM(BPF_H, dnp->dn_reg, 8, 0x003e));
}

static void
dt_cg_subr_link_ntop(dt_node_t *dnp, dt_irlist_t *dlp, dt_regset_t *drp)
{
	dt_node_t	*type = dnp->dn_args;
	uint_t		Lokay = dt_irlist_label(dlp);

	/* Determine type and check it is okay. */
	dt_cg_node(type, dlp, drp);
	emit(dlp,  BPF_BRANCH_IMM(BPF_JEQ, type->dn_reg, ARPHRD_INFINIBAND, Lokay));
	emit(dlp,  BPF_BRANCH_IMM(BPF_JEQ, type->dn_reg, ARPHRD_ETHER, Lokay));
	dt_cg_probe_error(yypcb, DTRACEFLT_ILLOP, DT_ISREG, type->dn_reg);
	emitl(dlp, Lokay,
		   BPF_NOP());

	dt_cg_subr_arg_to_tstring(dnp, dlp, drp, "dt_link_ntop", 1,
				  DT_ISREG, type->dn_reg, DT_IGNOR, 0);
	dt_regset_free(drp, type->dn_reg);
}

typedef void dt_cg_subr_f(dt_node_t *, dt_irlist_t *, dt_regset_t *);

static dt_cg_subr_f *_dt_cg_subr[DIF_SUBR_MAX + 1] = {
	[DIF_SUBR_RAND]			= &dt_cg_subr_rand,
	[DIF_SUBR_MUTEX_OWNED]		= &dt_cg_subr_mutex_owned,
	[DIF_SUBR_MUTEX_OWNER]		= &dt_cg_subr_mutex_owner,
	[DIF_SUBR_MUTEX_TYPE_ADAPTIVE]	= &dt_cg_subr_mutex_type_adaptive,
	[DIF_SUBR_MUTEX_TYPE_SPIN]	= &dt_cg_subr_mutex_type_spin,
	[DIF_SUBR_RW_READ_HELD]		= &dt_cg_subr_rw_read_held,
	[DIF_SUBR_RW_WRITE_HELD]	= &dt_cg_subr_rw_write_held,
	[DIF_SUBR_RW_ISWRITER]		= &dt_cg_subr_rw_iswriter,
	[DIF_SUBR_COPYIN]		= &dt_cg_subr_copyin,
	[DIF_SUBR_COPYINSTR]		= &dt_cg_subr_copyinstr,
	[DIF_SUBR_SPECULATION]		= &dt_cg_subr_speculation,
	[DIF_SUBR_PROGENYOF]		= &dt_cg_subr_progenyof,
	[DIF_SUBR_STRLEN]		= &dt_cg_subr_strlen,
	[DIF_SUBR_COPYOUT]		= &dt_cg_subr_copyout,
	[DIF_SUBR_COPYOUTSTR]		= &dt_cg_subr_copyoutstr,
	[DIF_SUBR_ALLOCA]		= &dt_cg_subr_alloca,
	[DIF_SUBR_BCOPY]		= &dt_cg_subr_bcopy,
	[DIF_SUBR_COPYINTO]		= &dt_cg_subr_copyinto,
	[DIF_SUBR_MSGDSIZE]		= NULL,
	[DIF_SUBR_MSGSIZE]		= NULL,
	[DIF_SUBR_GETMAJOR]		= &dt_cg_subr_getmajor,
	[DIF_SUBR_GETMINOR]		= &dt_cg_subr_getminor,
	[DIF_SUBR_DDI_PATHNAME]		= NULL,
	[DIF_SUBR_STRJOIN]		= dt_cg_subr_strjoin,
	[DIF_SUBR_LLTOSTR]		= &dt_cg_subr_lltostr,
	[DIF_SUBR_BASENAME]		= &dt_cg_subr_basename,
	[DIF_SUBR_DIRNAME]		= &dt_cg_subr_dirname,
	[DIF_SUBR_CLEANPATH]		= &dt_cg_subr_cleanpath,
	[DIF_SUBR_STRCHR]		= &dt_cg_subr_strchr,
	[DIF_SUBR_STRRCHR]		= &dt_cg_subr_strrchr,
	[DIF_SUBR_STRSTR]		= &dt_cg_subr_strstr,
	[DIF_SUBR_STRTOK]		= &dt_cg_subr_strtok,
	[DIF_SUBR_SUBSTR]		= &dt_cg_subr_substr,
	[DIF_SUBR_INDEX]		= &dt_cg_subr_index,
	[DIF_SUBR_RINDEX]		= &dt_cg_subr_rindex,
	[DIF_SUBR_HTONS]		= &dt_cg_subr_htons,
	[DIF_SUBR_HTONL]		= &dt_cg_subr_htonl,
	[DIF_SUBR_HTONLL]		= &dt_cg_subr_htonll,
	[DIF_SUBR_NTOHS]		= &dt_cg_subr_htons,
	[DIF_SUBR_NTOHL]		= &dt_cg_subr_htonl,
	[DIF_SUBR_NTOHLL]		= &dt_cg_subr_htonll,
	[DIF_SUBR_INET_NTOP]		= &dt_cg_subr_inet_ntop,
	[DIF_SUBR_INET_NTOA]		= &dt_cg_subr_inet_ntoa,
	[DIF_SUBR_INET_NTOA6]		= &dt_cg_subr_inet_ntoa6,
	[DIF_SUBR_D_PATH]		= &dt_cg_subr_d_path,
	[DIF_SUBR_LINK_NTOP]		= &dt_cg_subr_link_ntop,
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

	assert(idp->di_id <= DIF_SUBR_MAX);

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
		dt_cg_incdec_op(dnp, dlp, drp, BPF_ADD, 0);
		break;

	case DT_TOK_POSTINC:
		dt_cg_incdec_op(dnp, dlp, drp, BPF_ADD, 1);
		break;

	case DT_TOK_PREDEC:
		dt_cg_incdec_op(dnp, dlp, drp, BPF_SUB, 0);
		break;

	case DT_TOK_POSTDEC:
		dt_cg_incdec_op(dnp, dlp, drp, BPF_SUB, 1);
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
			uint_t	op;
			ssize_t	size;

			dt_cg_check_notnull(dlp, drp, dnp->dn_reg);
			op = dt_cg_ldsize(dnp, ctfp, dnp->dn_type, &size);

			/*
			 * If the child is an alloca pointer, bounds-check it
			 * now.
			 */
			if (dnp->dn_child->dn_flags & DT_NF_ALLOCA) {
				assert(!(dnp->dn_flags & DT_NF_ALLOCA));
				dt_cg_alloca_access_check(dlp, drp, dnp->dn_reg,
							  DT_ISIMM, size);
				dt_cg_alloca_ptr(dlp, drp, dnp->dn_reg,
						 dnp->dn_reg);
			}

			if (dnp->dn_child->dn_flags & (DT_NF_ALLOCA | DT_NF_DPTR))
				emit(dlp, BPF_LOAD(op, dnp->dn_reg, dnp->dn_reg, 0));
			else
				dt_cg_load_scalar(dnp, op, size, dlp, drp);
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
		dt_cg_check_notnull(dlp, drp, dnp->dn_child->dn_reg);
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

			/* Move tstring (if any) to dnp. */
			dnp->dn_tstring = mnp->dn_membexpr->dn_tstring;
			mnp->dn_membexpr->dn_tstring = NULL;

			dt_cg_typecast(mnp->dn_membexpr, dnp, dlp, drp);

			dxp->dx_ident->di_flags &= ~DT_IDFLG_CGREG;
			dxp->dx_ident->di_id = 0;

			if (dnp->dn_left->dn_reg != -1)
				dt_regset_free(drp, dnp->dn_left->dn_reg);
			break;
		}

		dt_cg_check_notnull(dlp, drp, dnp->dn_left->dn_reg);
		dnp->dn_reg = dnp->dn_left->dn_reg;

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

		if (dnp->dn_flags & DT_NF_BITFIELD) {
			dt_cg_field_get(dnp, dlp, drp, ctfp, &m);
			break;
		}

		if (m.ctm_offset != 0)
			emit(dlp, BPF_ALU64_IMM(BPF_ADD, dnp->dn_reg, m.ctm_offset / NBBY));

		if (!(dnp->dn_flags & DT_NF_REF)) {
			uint_t	op;
			ssize_t	size;

			op = dt_cg_ldsize(dnp, ctfp, m.ctm_type, &size);

			/*
			 * If the left-hand side of PTR or DOT is an alloca
			 * pointer, bounds-check the pointer reference.
			 */
			if (dnp->dn_left->dn_flags & DT_NF_ALLOCA) {
				assert(dnp->dn_flags & DT_NF_ALLOCA);
				dt_cg_alloca_access_check(dlp, drp, dnp->dn_reg,
							  DT_ISIMM, size);
				dt_cg_alloca_ptr(dlp, drp, dnp->dn_reg,
						 dnp->dn_reg);
			}

			if (dnp->dn_left->dn_flags & (DT_NF_ALLOCA | DT_NF_DPTR))
				emit(dlp, BPF_LOAD(op, dnp->dn_reg, dnp->dn_reg, 0));
			else
				dt_cg_load_scalar(dnp->dn_left, op, size, dlp, drp);
		}

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
		dt_cg_access_dctx(dnp->dn_reg, dlp, drp, DCTX_STRTAB);
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

			if (dnp->dn_ident->di_kind == DT_IDENT_ARRAY &&
			    dnp->dn_ident->di_id <= DIF_VAR_ARRAY_MAX) {
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

			dt_cg_xsetx(dlp, dnp->dn_ident, DT_LBL_NONE,
				    dnp->dn_reg, sym.st_value);

			if (!(dnp->dn_flags & DT_NF_REF)) {
				uint_t	op;
				ssize_t	size;

				op = dt_cg_ldsize(dnp, ctfp, dnp->dn_type, &size);

				dt_cg_load_scalar(dnp, op, size, dlp, drp);
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
 * We make room for a data counter of sizeof(uint64_t).
 */
#define DT_CG_AGG_SET_STORAGE(aid, sz) \
	do { \
		if ((aid)->di_offset == -1) \
			dt_ident_set_storage((aid), sizeof(uint64_t), \
					     sizeof(uint64_t) + (sz)); \
	} while (0)

#define DT_CG_AGG_IMPL(aid, sz, dlp, drp, f, ...) \
	do {								\
		int	dreg;						\
									\
		TRACE_REGSET("        Upd: Begin ");			\
									\
		if ((dreg = dt_regset_alloc(drp)) == -1)		\
			longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);		\
									\
		dt_cg_pop_stack(dreg, (dlp), (drp));			\
		(f)((dlp), (drp), dreg, ## __VA_ARGS__);		\
		dt_regset_free((drp), dreg);				\
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
	emit(dlp,  BPF_BRANCH_REG(BPF_JLE, lmdreg, lowreg, Lncy));
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
	dtrace_hdl_t	*dtp = pcb->pcb_hdl;
	dt_ident_t	*aid, *fid;
	dt_ident_t	*idp = dt_dlib_get_func(yypcb->pcb_hdl, "dt_get_agg");
	dt_cg_aggfunc_f	*aggfp;
	dt_node_t	noargs = {
					dnp->dn_ctfp,
					dtp->dt_type_void,
				 };
	uint_t		Lno_agg = dt_irlist_label(dlp);

	assert(idp != NULL);

	dt_cg_clsflags(pcb, DTRACEACT_AGGREGATION, dnp);

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

	/* Generate BPF code for the aggregation key (if any). */
	if (dnp->dn_aggtup == NULL)
		dnp->dn_aggtup = &noargs;

	dt_cg_arglist(aid, dnp->dn_aggtup, dlp, drp);

	/* Get the aggregation data pointer. */
	if (dt_regset_xalloc_args(drp) == -1)
		longjmp(yypcb->pcb_jmpbuf, EDT_NOREG);

	dt_cg_access_dctx(BPF_REG_1, dlp, drp, -1);
	emit(dlp,  BPF_MOV_IMM(BPF_REG_2, aid->di_id));
	emit(dlp,  BPF_MOV_REG(BPF_REG_3, dnp->dn_aggtup->dn_reg));
	dt_regset_free(drp, dnp->dn_aggtup->dn_reg);
	if (dnp->dn_aggtup == &noargs)
		dnp->dn_aggtup = NULL;

	switch (fid->di_id) {
	case DT_AGG_MAX:
		dt_cg_setx(dlp, BPF_REG_4, INT64_MIN);
		break;
	case DT_AGG_MIN:
		dt_cg_setx(dlp, BPF_REG_4, INT64_MAX);
		break;
	default:
		emit(dlp, BPF_MOV_IMM(BPF_REG_4, 0));
	}

	dt_cg_zerosptr(BPF_REG_5, dlp, drp);

	dt_regset_xalloc(drp, BPF_REG_0);
	emite(dlp, BPF_CALL_FUNC(idp->di_id), idp);
	dt_regset_free_args(drp);

	emit(dlp,  BPF_BRANCH_IMM(BPF_JEQ, BPF_REG_0, 0, Lno_agg));
	dt_regset_free(drp, BPF_REG_0);

	/* Push the agg data pointer onto the stack. */
	dt_cg_push_stack(BPF_REG_0, dlp, drp);

	/* Evaluate the aggregation function. */
	assert(fid->di_id >= DT_AGG_BASE && fid->di_id < DT_AGG_HIGHEST);
	aggfp = _dt_cg_agg[DT_AGG_IDX(fid->di_id)];
	assert(aggfp != NULL);
	(*aggfp)(pcb, aid, dnp, dlp, drp);

	/*
	 * Add this aggid if we see it for the first time.  We do this after
	 * the BPF code generation because aggfp() sets aid->di_size.
	 */
	if (dt_aggid_lookup(dtp, aid->di_id, NULL) == -1) {
		dt_node_t	*knp;

		dt_aggid_add(dtp, aid);

		for (knp = dnp->dn_aggtup; knp != NULL; knp = knp->dn_list) {
			size_t	size;
			int16_t	alignment;
			dtrace_actkind_t kind = DTRACEACT_DIFEXPR;
			uint64_t arg = 0;
			dt_ident_t *idp = knp->dn_ident;

			if (knp->dn_kind == DT_NODE_FUNC && idp != NULL &&
			    idp->di_kind == DT_IDENT_ACTFUNC) {
				size = 16;
				alignment = 8;
				switch (idp->di_id) {
				case DT_ACT_USTACK:
					arg = dt_cg_stack_arg(dtp, knp, DTRACEACT_USTACK);
					kind = DTRACEACT_USTACK;
					size = 8 + 8 * DTRACE_USTACK_NFRAMES(arg);
					break;
				case DT_ACT_JSTACK:
					kind = DTRACEACT_JSTACK;
					break;
				case DT_ACT_USYM:
					kind = DTRACEACT_USYM;
					break;
				case DT_ACT_UMOD:
					kind = DTRACEACT_UMOD;
					break;
				case DT_ACT_UADDR:
					kind = DTRACEACT_UADDR;
					break;
				case DT_ACT_STACK:
					arg = dt_cg_stack_arg(dtp, knp, DTRACEACT_STACK);
					kind = DTRACEACT_STACK;
					size = 8 * arg;
					break;
				case DT_ACT_SYM:
					kind = DTRACEACT_SYM;
					size = 8;
					break;
				case DT_ACT_MOD:
					kind = DTRACEACT_MOD;
					size = 8;
					break;
				}
			} else if (dt_node_is_string(knp)) {
				size = dtp->dt_options[DTRACEOPT_STRSIZE] + 1;
				alignment = 1;
			} else {
				dtrace_diftype_t	t;

				dt_node_diftype(dtp, knp, &t);
				size = t.dtdt_size;
				alignment = size;
			}

			dt_aggid_rec_add(dtp, aid->di_id, kind, size, alignment, arg);
		}
	}

	emitl(dlp, Lno_agg,
		   BPF_NOP());
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
	dt_cg_tstring_reset(pcb->pcb_hdl);

	dt_irlist_destroy(&pcb->pcb_ir);
	dt_irlist_create(&pcb->pcb_ir);
	pcb->pcb_exitlbl = dt_irlist_label(&pcb->pcb_ir);
	pcb->pcb_fastlbl = dt_irlist_label(&pcb->pcb_ir);

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
		int nonspec_acts = 0;
		int speculative = 0;

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

				if (actdp->kind == DTRACEACT_SPECULATE)
					speculative = 1;
				else
					nonspec_acts++;
				break;
			}
			case DT_NODE_AGG:
				dt_cg_agg(pcb, act, &pcb->pcb_ir,
					  pcb->pcb_regs);
				break;
			case DT_NODE_DEXPR: {
				dt_node_t	*enp = act->dn_expr;

				if (enp->dn_kind == DT_NODE_AGG)
					dt_cg_agg(pcb, enp, &pcb->pcb_ir,
						  pcb->pcb_regs);
				else
					dt_cg_node(enp, &pcb->pcb_ir,
						   pcb->pcb_regs);

				if (enp->dn_reg != -1) {
					dt_regset_free(pcb->pcb_regs,
						       enp->dn_reg);
					dt_cg_tstring_free(pcb, enp);
				}
				break;
			}
			default:
				dnerror(dnp, D_UNKNOWN, "internal error -- "
					"node kind %u is not a valid "
					"statement\n", dnp->dn_kind);
			}
		}
		if (dnp->dn_acts == NULL ||
		    (speculative && nonspec_acts == 0))
			pcb->pcb_stmt->dtsd_clauseflags |= DT_CLSFLAG_DATAREC;

		dt_cg_epilogue(pcb);
	} else if (dnp->dn_kind == DT_NODE_TRAMPOLINE) {
		assert(pcb->pcb_probe != NULL);

		if (pcb->pcb_probe->prov->impl->trampoline != NULL)
			pcb->pcb_probe->prov->impl->trampoline(pcb, pcb->pcb_exitlbl);
	} else
		dt_cg_node(dnp, &pcb->pcb_ir, pcb->pcb_regs);

	if (dnp->dn_kind == DT_NODE_MEMBER) {
		dt_regset_free(pcb->pcb_regs, dxp->dx_ident->di_id);
		dxp->dx_ident->di_id = 0;
		dxp->dx_ident->di_flags &= ~DT_IDFLG_CGREG;
	}
}
