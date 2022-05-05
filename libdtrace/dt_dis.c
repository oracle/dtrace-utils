/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <string.h>
#include <stdio.h>

#include <dt_impl.h>
#include <dt_dctx.h>
#include <dt_ident.h>
#include <dt_printf.h>
#include <dt_string.h>
#include <bpf_asm.h>
#include <port.h>

#define BPF_HELPER_FN(x)	[BPF_FUNC_##x] = __stringify(bpf_##x)
static const char * const helper_fn[] = {
	__BPF_FUNC_MAPPER(BPF_HELPER_FN)
};
#undef BPF_HELPER_FN

#define DT_DIS_INSTR_LEN	30
#define DT_DIS_PAD(n)		((n) > DT_DIS_INSTR_LEN \
					? 0 \
					: DT_DIS_INSTR_LEN - (n))

static void
dt_dis_prefix(uint_t i, const struct bpf_insn *instr, FILE *fp)
{
	fprintf(fp, "%04u %05u: %02hhx %01hhx %01hhx %04hx %08x    ",
		i, i*8, instr->code, instr->dst_reg, instr->src_reg,
		instr->off, instr->imm);
}

static const char *
reg(int r)
{
	static char	*name[] = { "%r0", "%r1", "%r2", "%r3", "%r4",
				    "%r5", "%r6", "%r7", "%r8", "%r9", "%fp" };

	return name[r];
}

static const char *
dt_dis_varname_id(const dtrace_difo_t *dp, uint_t id, uint_t scope, uint_t addr)
{
	const dtrace_difv_t *dvp = dp->dtdo_vartab;
	uint_t i;

	id &= 0x0fff;
	for (i = 0; i < dp->dtdo_varlen; i++, dvp++) {
		if (dvp->dtdv_id == id && dvp->dtdv_scope == scope &&
		    dvp->dtdv_insn_from <= addr && addr <= dvp->dtdv_insn_to) {
			if (dvp->dtdv_name < dp->dtdo_strlen)
				return dt_difo_getstr(dp, dvp->dtdv_name);
			break;
		}
	}

	return NULL;
}

static const char *
dt_dis_varname_off(const dtrace_difo_t *dp, uint_t off, uint_t scope, uint_t addr)
{
	uint_t i;

	for (i = 0; i < dp->dtdo_varlen; i++) {
		const dtrace_difv_t *dvp = &dp->dtdo_vartab[i];
		if (dvp->dtdv_offset == off && dvp->dtdv_scope == scope &&
		    dvp->dtdv_insn_from <= addr && addr <= dvp->dtdv_insn_to) {
			if (dvp->dtdv_name < dp->dtdo_strlen)
				return dt_difo_getstr(dp, dvp->dtdv_name);
			break;
		}
	}

	return NULL;
}

/*
 * Check if we are loading from the gvar, lvar, or strtab BPF map.
 *
 * If we are loading a gvar or lvar, we want to report the variable name.
 * If we are loading a string constant, we want to report its value.
 *
 * For variables, the sequence of instructions we are looking for is:
 *         insn   code  dst  src    offset        imm
 *          -2:    ld   dst  %fp  DT_STK_DCTX  00000000
 *          -1:    ld   dst  dst  DCTX_*VARS   00000000
 *           0:    ld   dst  dst  var_offset   00000000
 *           0:    st   dst  src  var_offset   00000000
 *           0:    add  dst    0     0         var_offset
 * where instruction 0 is the current instruction, which may be one
 * of the three above cases.  The three cases represent:
 *   - load by value
 *   - store by value
 *   - access by reference
 *
 * For string constants, the sequence of instructions we are looking for is:
 *         insn   code  dst  src    offset        imm
 *          -2:    ld   dst  %fp  DT_STK_DCTX  00000000
 *          -1:    ld   dst  dst  DCTX_STRTAB  00000000
 *           0:    add  dst    0     0         var_offset
 * where instruction 0 is the current instruction.
 */
static void
dt_dis_refname(const dtrace_difo_t *dp, const struct bpf_insn *in, uint_t addr,
	       int n, FILE *fp)
{
	__u8		ldcode = BPF_LDX | BPF_MEM | BPF_DW;
	__u8		addcode = BPF_ALU64 | BPF_ADD | BPF_K;
	int		dst, scope = -1, var_offset = -1;
	const char	*str;

	/* make sure in[-2] and in[-1] exist */
	if (addr < 2)
		goto out;

	/* get dst and scope */
	dst = in[-1].dst_reg;

	if (in[-1].off == DCTX_GVARS)
		scope = DIFV_SCOPE_GLOBAL;
	else if (in[-1].off == DCTX_LVARS)
		scope = DIFV_SCOPE_LOCAL;
	else if (in[-1].off != DCTX_STRTAB)
		goto out;

	/* check preceding instructions */
	if (in[-2].code != ldcode ||
	    in[-2].dst_reg != dst ||
	    in[-2].src_reg != BPF_REG_FP ||
	    in[-2].off != DT_STK_DCTX ||
	    in[-2].imm != 0 ||
	    in[-1].code != ldcode ||
	    in[-1].src_reg != dst ||
	    in[-1].imm != 0)
		goto out;

	/* check the current instruction and read var_offset */
	if (in->dst_reg != dst)
		goto out;
	if (in[-1].off == DCTX_STRTAB) {
		if (in->code == addcode && in->src_reg == 0 && in->off == 0) {
			str = dt_difo_getstr(dp, in->imm);
			if (str != NULL)
				fprintf(fp, "%*s! \"%s\"", DT_DIS_PAD(n), "",
					str);
		}

		goto out;
	} else if (BPF_CLASS(in->code) == BPF_LDX &&
		   BPF_MODE(in->code) == BPF_MEM &&
		   in->src_reg == dst && in->imm == 0)
		var_offset = in->off;
	else if (BPF_CLASS(in->code) == BPF_STX &&
		 BPF_MODE(in->code) == BPF_MEM &&
		 in->src_reg != dst && in->imm == 0)
		var_offset = in->off;
	else if (in->code == addcode && in->src_reg == 0 && in->off == 0)
		var_offset = in->imm;
	else
		goto out;

	/* print name */
	str = dt_dis_varname_off(dp, var_offset, scope, addr);
	if (str != NULL)
		fprintf(fp, "%*s! %s%s", DT_DIS_PAD(n), "",
			scope == DIFV_SCOPE_LOCAL ? "this->" : "", str);

out:
	fprintf(fp, "\n");
}

/*ARGSUSED*/
static uint_t
dt_dis_str(const dtrace_difo_t *dp, const char *name, uint_t addr,
	   const struct bpf_insn *in, const char *rname, FILE *fp)
{
	fprintf(fp, "%s\n", name);
	return 0;
}

/*ARGSUSED*/
static uint_t
dt_dis_op1(const dtrace_difo_t *dp, const char *name, uint_t addr,
	   const struct bpf_insn *in, const char *rname, FILE *fp)
{
	fprintf(fp, "%-4s %s\n", name, reg(in->dst_reg));
	return 0;
}

/*ARGSUSED*/
static uint_t
dt_dis_op2(const dtrace_difo_t *dp, const char *name, uint_t addr,
	   const struct bpf_insn *in, const char *rname, FILE *fp)
{
	fprintf(fp, "%-4s %s, %s\n", name, reg(in->dst_reg), reg(in->src_reg));
	return 0;
}

/*ARGSUSED*/
static uint_t
dt_dis_op2imm(const dtrace_difo_t *dp, const char *name, uint_t addr,
	      const struct bpf_insn *in, const char *rname, FILE *fp)
{
	int		n;

	n = fprintf(fp, "%-4s %s, %d", name, reg(in->dst_reg), in->imm);
	dt_dis_refname(dp, in, addr, n, fp);

	return 0;
}

/*ARGSUSED*/
static uint_t
dt_dis_branch(const dtrace_difo_t *dp, const char *name, uint_t addr,
	      const struct bpf_insn *in, const char *rname, FILE *fp)
{
	int	n;

	n = fprintf(fp, "%-4s %s, %s, %d", name, reg(in->dst_reg),
		    reg(in->src_reg), in->off);
	fprintf(fp, "%*s! -> %03u\n", DT_DIS_PAD(n), "", addr + 1 + in->off);
	return 0;
}

/*ARGSUSED*/
static uint_t
dt_dis_branch_imm(const dtrace_difo_t *dp, const char *name, uint_t addr,
	      const struct bpf_insn *in, const char *rname, FILE *fp)
{
	int	n;

	n = fprintf(fp, "%-4s %s, %u, %d", name, reg(in->dst_reg), in->imm,
		    in->off);
	fprintf(fp, "%*s! -> %03u\n", DT_DIS_PAD(n), "", addr + 1 + in->off);
	return 0;
}

/*ARGSUSED*/
static uint_t
dt_dis_sbranch_imm(const dtrace_difo_t *dp, const char *name, uint_t addr,
	      const struct bpf_insn *in, const char *rname, FILE *fp)
{
	int	n;

	n = fprintf(fp, "%-4s %s, %d, %d", name, reg(in->dst_reg), in->imm,
		    in->off);
	fprintf(fp, "%*s! -> %03u\n", DT_DIS_PAD(n), "", addr + 1 + in->off);
	return 0;
}

/*ARGSUSED*/
static uint_t
dt_dis_load(const dtrace_difo_t *dp, const char *name, uint_t addr,
	      const struct bpf_insn *in, const char *rname, FILE *fp)
{
	int		n;

	n = fprintf(fp, "%-4s %s, [%s%+d]", name, reg(in->dst_reg),
		    reg(in->src_reg), in->off);

	if (in->code == (BPF_LDX | BPF_MEM | BPF_DW) &&
	    in->src_reg == BPF_REG_FP &&
	    in->off <= DT_STK_SPILL(0) &&
	    in->off >= DT_STK_SPILL(DT_STK_NREGS) &&
	    in->dst_reg == -(in->off - DT_STK_SPILL_BASE) / DT_STK_SLOT_SZ) {
		fprintf(fp, "%*s! restore %s\n", DT_DIS_PAD(n), "",
			reg(in->dst_reg));
	} else
		dt_dis_refname(dp, in, addr, n, fp);

	return 0;
}

/*ARGSUSED*/
static uint_t
dt_dis_load_imm(const dtrace_difo_t *dp, const char *name, uint_t addr,
		const struct bpf_insn *in, const char *rname, FILE *fp)
{
	/*
	 * A double word load-immediate instruction takes up two instruction
	 * slots in BPF code.
	 */
	fprintf(fp, "%-4s %s, 0x%08x%08x\n", name, reg(in->dst_reg),
		in[1].imm, in[0].imm);
	dt_dis_prefix(addr + 1, &in[1], fp);

	if (rname != NULL)
		fprintf(fp, "%*s! %s\n", DT_DIS_INSTR_LEN, "", rname);
	else
		fprintf(fp, "\n");

	return 1;
}

/*ARGSUSED*/
static uint_t
dt_dis_store(const dtrace_difo_t *dp, const char *name, uint_t addr,
	     const struct bpf_insn *in, const char *rname, FILE *fp)
{
	int		n;

	n = fprintf(fp, "%-4s [%s%+d], %s", name, reg(in->dst_reg), in->off,
		    reg(in->src_reg));

	if (in->code == (BPF_STX | BPF_MEM | BPF_DW) &&
	    in->dst_reg == BPF_REG_FP &&
	    in->off <= DT_STK_SPILL(0) &&
	    in->off >= DT_STK_SPILL(DT_STK_NREGS - 1) &&
	    in->src_reg == -(in->off - DT_STK_SPILL_BASE) / DT_STK_SLOT_SZ) {
		fprintf(fp, "%*s! spill %s\n", DT_DIS_PAD(n), "",
			reg(in->src_reg));
	} else
		dt_dis_refname(dp, in, addr, n, fp);

	return 0;
}

/*ARGSUSED*/
static uint_t
dt_dis_store_imm(const dtrace_difo_t *dp, const char *name, uint_t addr,
		 const struct bpf_insn *in, const char *rname, FILE *fp)
{
	int		n;

	n = fprintf(fp, "%-4s [%s%+d], %d", name, reg(in->dst_reg), in->off,
		    in->imm);

	if (rname)
		fprintf(fp, "%*s! = %s\n", DT_DIS_PAD(n), "", rname);
	else
		fprintf(fp, "\n");

	return 0;
}

static char *
dt_dis_bpf_args(const dtrace_difo_t *dp, const char *fn,
		const struct bpf_insn *in, char *buf, size_t len, uint_t addr)
{
	if (strcmp(fn, "dt_get_bvar") == 0) {
		/*
		 * We know that the previous two instructions exist and move
		 * the variable id to a register in the first instruction of
		 * that sequence (because we wrote the code generator to emit
		 * the instructions in this exact order.)
		 */
		in -= 2;
		snprintf(buf, len, "%s",
			 dt_dis_varname_id(dp, in->imm, DIFV_SCOPE_GLOBAL, addr));
		return buf;
	} else if (strcmp(fn, "dt_get_tvar") == 0) {
		/*
		 * We know that the previous three instructions exist and
		 * move the variable id to a register in the first instruction
		 * of that seqeuence (because we wrote the code generator to
		 * emit the instructions in this exact order.)
		 */
		in -= 3;
		snprintf(buf, len, "self->%s",
			 dt_dis_varname_id(dp, in->imm + DIF_VAR_OTHER_UBASE,
					DIFV_SCOPE_THREAD, addr));
		return buf;
	} else if (strcmp(fn, "dt_get_assoc") == 0) {
		uint_t		varid;

		/*
		 * We know that the previous four instructions exist and
		 * move the variable id to a register in the first instruction
		 * of that seqeuence (because we wrote the code generator to
		 * emit the instructions in this exact order.)
		 */
		in -= 4;
		varid = in->imm + DIF_VAR_OTHER_UBASE;

		/*
		 * Four instructions prior to this there will be a STORE (TLS
		 * associative array) or a STORE_IMM (global associative array).
		 */
		in -= 4;
		if (in->code == (BPF_STX | BPF_MEM | BPF_DW))
			snprintf(buf, len, "self->%s[]",
				 dt_dis_varname_id(dp, varid, DIFV_SCOPE_THREAD,
						   addr));
		else if (in->code == (BPF_ST | BPF_MEM | BPF_DW))
			snprintf(buf, len, "%s[]",
				 dt_dis_varname_id(dp, varid, DIFV_SCOPE_GLOBAL,
						   addr));
		else
			snprintf(buf, len, "???[]");

		return buf;
	}

	return NULL;
}

/*ARGSUSED*/
static uint_t
dt_dis_call(const dtrace_difo_t *dp, const char *name, uint_t addr,
	    const struct bpf_insn *in, const char *rname, FILE *fp)
{
	const char	*fn = NULL;
	const char	*ann = NULL;
	char		buf[256];

	/*
	 * Try to determine a name for the function being called.  We also call
	 * a function-type (built-in BPF vs helper) specific function in case
	 * there are annotations to be added.
	 */
	if (in->src_reg == BPF_PSEUDO_CALL) {
		fn = rname;
		if (fn == NULL) {
			snprintf(buf, sizeof(buf), "%d", in->imm);
			fn = buf;
		} else
			ann = dt_dis_bpf_args(dp, fn, in, buf, sizeof(buf),
					      addr);
	} else if (in->imm >= 0 && in->imm < __BPF_FUNC_MAX_ID) {
		fn = helper_fn[in->imm];
	} else {
		snprintf(buf, sizeof(buf), "helper#%d", in->imm);
		fn = buf;
		ann = "unknown helper";
	}

	if (ann) {
		int	n;

		n = fprintf(fp, "%-4s %s", name, fn);
		fprintf(fp, "%*s! %s\n", DT_DIS_PAD(n), "", ann);
	} else
		fprintf(fp, "%-4s %s\n", name, fn);

	return 0;
}

/*ARGSUSED*/
static uint_t
dt_dis_jump(const dtrace_difo_t *dp, const char *name, uint_t addr,
	    const struct bpf_insn *in, const char *rname, FILE *fp)
{
	if (in->off == 0)
		fprintf(fp, "nop\n");
	else {
		int	n;

		n = fprintf(fp, "%-4s %d", name, in->off);
		fprintf(fp, "%*s! -> %03u\n", DT_DIS_PAD(n), "",
			addr + 1 + in->off);
	}

	return 0;
}

static char *
dt_dis_typestr(const dtrace_diftype_t *t, char *buf, size_t len)
{
	char kind[16], ckind[16];

	switch (t->dtdt_kind) {
	case DIF_TYPE_CTF:
		strcpy(kind, "D type");
		break;
	case DIF_TYPE_STRING:
		strcpy(kind, "string");
		break;
	case DIF_TYPE_ANY:
		strcpy(kind, "any");
		break;
	default:
		snprintf(kind, sizeof(kind), "0x%x", t->dtdt_kind);
	}

	switch (t->dtdt_ckind) {
	case CTF_K_UNKNOWN:
		strcpy(ckind, "unknown");
		break;
	case CTF_K_INTEGER:
		strcpy(ckind, "integer");
		break;
	case CTF_K_FLOAT:
		strcpy(ckind, "float");
		break;
	case CTF_K_POINTER:
		strcpy(ckind, "pointer");
		break;
	case CTF_K_ARRAY:
		strcpy(ckind, "array");
		break;
	case CTF_K_FUNCTION:
		strcpy(ckind, "function");
		break;
	case CTF_K_STRUCT:
		strcpy(ckind, "struct");
		break;
	case CTF_K_UNION:
		strcpy(ckind, "union");
		break;
	case CTF_K_ENUM:
		strcpy(ckind, "enum");
		break;
	case CTF_K_FORWARD:
		strcpy(ckind, "forward");
		break;
	case CTF_K_TYPEDEF:
		strcpy(ckind, "typedef");
		break;
	case CTF_K_VOLATILE:
		strcpy(ckind, "volatile");
		break;
	case CTF_K_CONST:
		strcpy(ckind, "const");
		break;
	case CTF_K_RESTRICT:
		strcpy(ckind, "restrict");
		break;
	default:
		snprintf(ckind, sizeof(ckind), "0x%x", t->dtdt_ckind);
	}

	if (t->dtdt_flags & DIF_TF_BYREF) {
		snprintf(buf, len, "%s (%s) by ref (size %lu)",
		    kind, ckind, (ulong_t)t->dtdt_size);
	} else {
		snprintf(buf, len, "%s (%s) (size %lu)",
		    kind, ckind, (ulong_t)t->dtdt_size);
	}

	return buf;
}

static void
dt_dis_rtab(const char *rtag, const dtrace_difo_t *dp, FILE *fp,
    const dof_relodesc_t *rp, uint32_t len)
{
	fprintf(fp, "\n%-17s %-8s %-8s %s\n", rtag, "OFFSET", "VALUE", "NAME");

	for (; len != 0; len--, rp++) {
		const char	*tstr;

		switch (rp->dofr_type) {
		case R_BPF_NONE:
			tstr = "R_BPF_NONE";
			break;
		case R_BPF_64_64:
			tstr = "R_BPF_INSN_64";
			break;
		case R_BPF_64_32:
			tstr = "R_BPF_INSN_DISP32";
			break;
		default:
			tstr = "R_???";
		}

		if ((u_longlong_t)rp->dofr_data != DT_IDENT_UNDEF)
			fprintf(fp, "%-17s %-8llu %-8llu %s\n", tstr,
				(u_longlong_t)rp->dofr_offset,
				(u_longlong_t)rp->dofr_data,
				dt_difo_getstr(dp, rp->dofr_name));
		else
			fprintf(fp, "%-17s %-8llu %-8s %s\n", tstr,
				(u_longlong_t)rp->dofr_offset,
				"*UND*",
				dt_difo_getstr(dp, rp->dofr_name));
	}
}

static const struct opent {
	const char *op_name;
	uint_t (*op_func)(const dtrace_difo_t *, const char *, uint_t,
			  const struct bpf_insn *, const char *,
			  FILE *);
} optab[256] = {
	[0 ... 255] = { "(illegal opcode)", dt_dis_str },
#define INSN2(x, y)	[BPF_##x | BPF_##y]
#define INSN3(x, y, z)	[BPF_##x | BPF_##y | BPF_##z]
	/* 32-bit ALU ops, op(dst, src) */
	INSN3(ALU, ADD, X)	= { "add", dt_dis_op2 },
	INSN3(ALU, SUB, X)	= { "sub", dt_dis_op2 },
	INSN3(ALU, AND, X)	= { "and", dt_dis_op2 },
	INSN3(ALU, OR, X)	= { "or", dt_dis_op2 },
	INSN3(ALU, LSH, X)	= { "lsh", dt_dis_op2 },
	INSN3(ALU, RSH, X)	= { "rsh", dt_dis_op2 },
	INSN3(ALU, XOR, X)	= { "xor", dt_dis_op2 },
	INSN3(ALU, MUL, X)	= { "mul", dt_dis_op2 },
	INSN3(ALU, MOV, X)	= { "mov", dt_dis_op2 },
	INSN3(ALU, ARSH, X)	= { "arsh", dt_dis_op2 },
	INSN3(ALU, DIV, X)	= { "div", dt_dis_op2 },
	INSN3(ALU, MOD, X)	= { "mod", dt_dis_op2 },
	INSN2(ALU, NEG)		= { "neg", dt_dis_op1 },
	INSN3(ALU, END, TO_BE)	= { "tobe", dt_dis_op2 },
	INSN3(ALU, END, TO_LE)	= { "tole", dt_dis_op2 },
	/* 32-bit ALU ops, op(dst, imm) */
	INSN3(ALU, ADD, K)	= { "add", dt_dis_op2imm },
	INSN3(ALU, SUB, K)	= { "sub", dt_dis_op2imm },
	INSN3(ALU, AND, K)	= { "and", dt_dis_op2imm },
	INSN3(ALU, OR, K)	= { "or", dt_dis_op2imm },
	INSN3(ALU, LSH, K)	= { "lsh", dt_dis_op2imm },
	INSN3(ALU, RSH, K)	= { "rsh", dt_dis_op2imm },
	INSN3(ALU, XOR, K)	= { "xor", dt_dis_op2imm },
	INSN3(ALU, MUL, K)	= { "mul", dt_dis_op2imm },
	INSN3(ALU, MOV, K)	= { "mov", dt_dis_op2imm },
	INSN3(ALU, ARSH, K)	= { "arsh", dt_dis_op2imm },
	INSN3(ALU, DIV, K)	= { "div", dt_dis_op2imm },
	INSN3(ALU, MOD, K)	= { "mod", dt_dis_op2imm },
	/* 64-bit ALU ops, op(dst, src) */
	INSN3(ALU64, ADD, X)	= { "add", dt_dis_op2 },
	INSN3(ALU64, SUB, X)	= { "sub", dt_dis_op2 },
	INSN3(ALU64, AND, X)	= { "and", dt_dis_op2 },
	INSN3(ALU64, OR, X)	= { "or", dt_dis_op2 },
	INSN3(ALU64, LSH, X)	= { "lsh", dt_dis_op2 },
	INSN3(ALU64, RSH, X)	= { "rsh", dt_dis_op2 },
	INSN3(ALU64, XOR, X)	= { "xor", dt_dis_op2 },
	INSN3(ALU64, MUL, X)	= { "mul", dt_dis_op2 },
	INSN3(ALU64, MOV, X)	= { "mov", dt_dis_op2 },
	INSN3(ALU64, ARSH, X)	= { "arsh", dt_dis_op2 },
	INSN3(ALU64, DIV, X)	= { "div", dt_dis_op2 },
	INSN3(ALU64, MOD, X)	= { "mod", dt_dis_op2 },
	INSN2(ALU64, NEG)	= { "neg", dt_dis_op1 },
	/* 64-bit ALU ops, op(dst, imm) */
	INSN3(ALU64, ADD, K)	= { "add", dt_dis_op2imm },
	INSN3(ALU64, SUB, K)	= { "sub", dt_dis_op2imm },
	INSN3(ALU64, AND, K)	= { "and", dt_dis_op2imm },
	INSN3(ALU64, OR, K)	= { "or", dt_dis_op2imm },
	INSN3(ALU64, LSH, K)	= { "lsh", dt_dis_op2imm },
	INSN3(ALU64, RSH, K)	= { "rsh", dt_dis_op2imm },
	INSN3(ALU64, XOR, K)	= { "xor", dt_dis_op2imm },
	INSN3(ALU64, MUL, K)	= { "mul", dt_dis_op2imm },
	INSN3(ALU64, MOV, K)	= { "mov", dt_dis_op2imm },
	INSN3(ALU64, ARSH, K)	= { "arsh", dt_dis_op2imm },
	INSN3(ALU64, DIV, K)	= { "div", dt_dis_op2imm },
	INSN3(ALU64, MOD, K)	= { "mod", dt_dis_op2imm },
	/* Call instruction */
	INSN2(JMP, CALL)	= { "call", dt_dis_call },
	/* Return instruction */
	INSN2(JMP, EXIT)	= { "exit", dt_dis_str },
	/* 32-bit jump ops, op(dst, src) */
	INSN3(JMP32, JEQ, X)	= { "jeq", dt_dis_branch },
	INSN3(JMP32, JNE, X)	= { "jne", dt_dis_branch },
	INSN3(JMP32, JGT, X)	= { "jgt", dt_dis_branch },
	INSN3(JMP32, JLT, X)	= { "jlt", dt_dis_branch },
	INSN3(JMP32, JGE, X)	= { "jge", dt_dis_branch },
	INSN3(JMP32, JLE, X)	= { "jle", dt_dis_branch },
	INSN3(JMP32, JSGT, X)	= { "jsgt", dt_dis_branch },
	INSN3(JMP32, JSLT, X)	= { "jslt", dt_dis_branch },
	INSN3(JMP32, JSGE, X)	= { "jsge", dt_dis_branch },
	INSN3(JMP32, JSLE, X)	= { "jsle", dt_dis_branch },
	INSN3(JMP32, JSET, X)	= { "jset", dt_dis_branch },
	/* 32-bit jump ops, op(dst, imm) */
	INSN3(JMP32, JEQ, K)	= { "jeq", dt_dis_branch_imm },
	INSN3(JMP32, JNE, K)	= { "jne", dt_dis_branch_imm },
	INSN3(JMP32, JGT, K)	= { "jgt", dt_dis_branch_imm },
	INSN3(JMP32, JLT, K)	= { "jlt", dt_dis_branch_imm },
	INSN3(JMP32, JGE, K)	= { "jge", dt_dis_branch_imm },
	INSN3(JMP32, JLE, K)	= { "jle", dt_dis_branch_imm },
	INSN3(JMP32, JSGT, K)	= { "jsgt", dt_dis_sbranch_imm },
	INSN3(JMP32, JSLT, K)	= { "jslt", dt_dis_sbranch_imm },
	INSN3(JMP32, JSGE, K)	= { "jsge", dt_dis_sbranch_imm },
	INSN3(JMP32, JSLE, K)	= { "jsle", dt_dis_sbranch_imm },
	INSN3(JMP32, JSET, K)	= { "jset", dt_dis_branch_imm },
	/* 64-bit jump ops, op(dst, src) */
	INSN3(JMP, JEQ, X)	= { "jeq", dt_dis_branch },
	INSN3(JMP, JNE, X)	= { "jne", dt_dis_branch },
	INSN3(JMP, JGT, X)	= { "jgt", dt_dis_branch },
	INSN3(JMP, JLT, X)	= { "jlt", dt_dis_branch },
	INSN3(JMP, JGE, X)	= { "jge", dt_dis_branch },
	INSN3(JMP, JLE, X)	= { "jle", dt_dis_branch },
	INSN3(JMP, JSGT, X)	= { "jsgt", dt_dis_branch },
	INSN3(JMP, JSLT, X)	= { "jslt", dt_dis_branch },
	INSN3(JMP, JSGE, X)	= { "jsge", dt_dis_branch },
	INSN3(JMP, JSLE, X)	= { "jsle", dt_dis_branch },
	INSN3(JMP, JSET, X)	= { "jset", dt_dis_branch },
	/* 64-bit jump ops, op(dst, imm) */
	INSN3(JMP, JEQ, K)	= { "jeq", dt_dis_branch_imm },
	INSN3(JMP, JNE, K)	= { "jne", dt_dis_branch_imm },
	INSN3(JMP, JGT, K)	= { "jgt", dt_dis_branch_imm },
	INSN3(JMP, JLT, K)	= { "jlt", dt_dis_branch_imm },
	INSN3(JMP, JGE, K)	= { "jge", dt_dis_branch_imm },
	INSN3(JMP, JLE, K)	= { "jle", dt_dis_branch_imm },
	INSN3(JMP, JSGT, K)	= { "jsgt", dt_dis_sbranch_imm },
	INSN3(JMP, JSLT, K)	= { "jslt", dt_dis_sbranch_imm },
	INSN3(JMP, JSGE, K)	= { "jsge", dt_dis_sbranch_imm },
	INSN3(JMP, JSLE, K)	= { "jsle", dt_dis_sbranch_imm },
	INSN3(JMP, JSET, K)	= { "jset", dt_dis_branch_imm },
	INSN2(JMP, JA)		= { "ja", dt_dis_jump },
	/* Store instructions, [dst + off] = src */
	INSN3(STX, MEM, B)	= { "stb", dt_dis_store },
	INSN3(STX, MEM, H)	= { "sth", dt_dis_store },
	INSN3(STX, MEM, W)	= { "stw", dt_dis_store },
	INSN3(STX, MEM, DW)	= { "stdw", dt_dis_store },
	INSN3(STX, XADD, W)	= { "xadd", dt_dis_store },
	INSN3(STX, XADD, DW)	= { "xadd", dt_dis_store },
	/* Store instructions, [dst + off] = imm */
	INSN3(ST, MEM, B)	= { "stb", dt_dis_store_imm },
	INSN3(ST, MEM, H)	= { "sth", dt_dis_store_imm },
	INSN3(ST, MEM, W)	= { "stw", dt_dis_store_imm },
	INSN3(ST, MEM, DW)	= { "stdw", dt_dis_store_imm },
	/* Load instructions, dst = [src + off] */
	INSN3(LDX, MEM, B)	= { "ldb", dt_dis_load },
	INSN3(LDX, MEM, H)	= { "ldh", dt_dis_load },
	INSN3(LDX, MEM, W)	= { "ldw", dt_dis_load },
	INSN3(LDX, MEM, DW)	= { "lddw", dt_dis_load },
	/* Load instructions, dst = imm */
	INSN3(LD, IMM, DW)	= { "lddw", dt_dis_load_imm },
};

void
dt_dis_insn(uint_t i, const struct bpf_insn *instr, FILE *fp)
{
	uint8_t			opcode = instr->code;
	const struct opent	*op;

	if (opcode >= ARRAY_SIZE(optab))
		opcode = 0;	/* force invalid opcode message */

	dt_dis_prefix(i, instr, fp);

	op = &optab[opcode];
	op->op_func(NULL, op->op_name, i, instr, NULL, fp);
}

void
dt_dis_difo(const dtrace_difo_t *dp, FILE *fp, const dt_ident_t *idp,
	    const dtrace_probedesc_t *pdp, const char *ltype)
{
	uint_t		i = 0;
	dof_relodesc_t	*rp = dp->dtdo_breltab;
	int		cnt = dp->dtdo_brelen;
	char		type[DT_TYPE_NAMELEN];

	if (pdp != NULL && idp != NULL)
		fprintf(fp, "\nDisassembly of %s %s:%s:%s:%s, <%s>:\n", ltype,
			pdp->prv, pdp->mod, pdp->fun, pdp->prb, idp->di_name);
	else if (pdp != NULL)
		fprintf(fp, "\nDisassembly of %s %s:%s:%s:%s:\n", ltype,
			pdp->prv, pdp->mod, pdp->fun, pdp->prb);
	else if (idp != NULL)
		fprintf(fp, "\nDisassembly of %s <%s>:\n", ltype, idp->di_name);
	else
		fprintf(fp, "\nDisassembly of %s:\n", ltype);

	fprintf(fp, "%-4s %-5s  %-20s    %s\n",
	    "INS", "OFF", "OPCODE", "INSTRUCTION");

	for (i = 0; i < dp->dtdo_len; i++) {
		const struct bpf_insn	*instr = &dp->dtdo_buf[i];
		uint8_t			opcode = instr->code;
		const struct opent	*op;
		const char		*rname = NULL;
		uint_t			skip;

		if (opcode >= ARRAY_SIZE(optab))
			opcode = 0; /* force invalid opcode message */

		dt_dis_prefix(i, instr, fp);

		/*
		 * If there are remaining relocations, and the next relocation
		 * applies to the current instruction, pass the associated
		 * symbol name to the instruction handler.
		 */
		for (; cnt; cnt--, rp++) {
			if (rp->dofr_offset < i * sizeof(uint64_t))
				continue;
			if (rp->dofr_offset == i * sizeof(uint64_t))
				rname = dt_difo_getstr(dp, rp->dofr_name);

			break;
		}

		op = &optab[opcode];
		skip = op->op_func(dp, op->op_name, i, instr, rname, fp);

		/*
		 * If the instruction handler indicated we need to skip some
		 * additional instruction(s), we better listen...
		 */
		if (skip)
			i += skip;
	}

	if (dp->dtdo_varlen != 0) {
		fprintf(fp, "\n%-16s %-4s %-6s %-3s %-3s %-11s %-4s %s\n",
			"NAME", "ID", "OFFSET", "KND", "SCP", "RANGE", "FLAG", "TYPE");
	}

	for (i = 0; i < dp->dtdo_varlen; i++) {
		dtrace_difv_t *v = &dp->dtdo_vartab[i];
		char offset[11] = { 0 }, flags[16] = { 0 };
		char kind[4], scope[4], range[12];

		if (v->dtdv_offset != -1)
			snprintf(offset, sizeof(offset), "%u", v->dtdv_offset);

		switch (v->dtdv_kind) {
		case DIFV_KIND_AGGREGATE:
			strcpy(kind, "agg");
			break;
		case DIFV_KIND_ARRAY:
			strcpy(kind, "arr");
			break;
		case DIFV_KIND_SCALAR:
			strcpy(kind, "scl");
			break;
		default:
			snprintf(kind, sizeof(kind), "%u", v->dtdv_kind);
		}

		switch (v->dtdv_scope) {
		case DIFV_SCOPE_GLOBAL:
			strcpy(scope, "glb");
			break;
		case DIFV_SCOPE_THREAD:
			strcpy(scope, "tls");
			break;
		case DIFV_SCOPE_LOCAL:
			strcpy(scope, "loc");
			break;
		default:
			snprintf(scope, sizeof(scope), "%u", v->dtdv_scope);
		}

		snprintf(range, sizeof(range), "[%u-%u]",
			 v->dtdv_insn_from, v->dtdv_insn_to);

		if (v->dtdv_flags & ~(DIFV_F_REF | DIFV_F_MOD)) {
			snprintf(flags, sizeof(flags), "/0x%x",
				 v->dtdv_flags & ~(DIFV_F_REF | DIFV_F_MOD));
		}

		if (v->dtdv_flags & DIFV_F_REF)
			strcat(flags, "/r");
		if (v->dtdv_flags & DIFV_F_MOD)
			strcat(flags, "/w");

		fprintf(fp, "%-16s %-4x %-6s %-3s %-3s %-11s %-4s %s\n",
			dt_difo_getstr(dp, v->dtdv_name), v->dtdv_id,
			offset, kind, scope, range, flags + 1,
			dt_dis_typestr(&v->dtdv_type, type, sizeof(type)));
	}

	if (dp->dtdo_xlmlen != 0) {
		fprintf(fp, "\n%-4s %-3s %-12s %s\n",
			"XLID", "ARG", "MEMBER", "TYPE");
	}

	for (i = 0; i < dp->dtdo_xlmlen; i++) {
		dt_node_t *dnp = dp->dtdo_xlmtab[i];
		dt_xlator_t *dxp = dnp->dn_membexpr->dn_xlator;
		fprintf(fp, "%-4u %-3d %-12s %s\n", (uint_t)dxp->dx_id,
			dxp->dx_arg, dnp->dn_membname,
			dt_node_type_name(dnp, type, sizeof(type)));
	}

	if (dp->dtdo_brelen != 0)
		dt_dis_rtab("BPF", dp, fp, dp->dtdo_breltab, dp->dtdo_brelen);

	if (dp->dtdo_krelen != 0)
		dt_dis_rtab("KREL", dp, fp, dp->dtdo_kreltab, dp->dtdo_krelen);

	if (dp->dtdo_urelen != 0)
		dt_dis_rtab("UREL", dp, fp, dp->dtdo_ureltab, dp->dtdo_urelen);

	fprintf(fp, "\n");
}

typedef struct dt_dis_iter
{
	FILE *fp;
	dtrace_ecbdesc_t *last_ecb;
} dt_dis_iter_t;

static int
dt_dis_stmts(dtrace_hdl_t *dtp, dtrace_prog_t *pgp, dtrace_stmtdesc_t *sdp,
    void *data)
{
	dt_dis_iter_t		*d = data;
	dtrace_probedesc_t	*pdp = NULL;

	if (d->last_ecb != sdp->dtsd_ecbdesc) {
		pdp = &sdp->dtsd_ecbdesc->dted_probe;
		d->last_ecb = sdp->dtsd_ecbdesc;
	}

	dt_dis_difo(dt_dlib_get_func_difo(dtp, sdp->dtsd_clause), d->fp,
		    sdp->dtsd_clause, pdp, "clause");

	return 0;
}

void
dt_dis_program(dtrace_hdl_t *dtp, dtrace_prog_t *pgp, FILE *fp)
{
	dt_dis_iter_t data = { fp };

	dtrace_stmt_iter(dtp, pgp, dt_dis_stmts, &data);
}
