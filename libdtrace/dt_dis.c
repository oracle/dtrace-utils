/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, 2019, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <string.h>
#include <stdio.h>

#include <dt_impl.h>
#include <dt_ident.h>
#include <dt_printf.h>
#include <dt_string.h>
#include <dt_bpf_builtins.h>
#include <bpf_asm.h>

#define BPF_HELPER_FN(x)	[BPF_FUNC_##x] = __stringify(bpf_##x)
static const char * const helper_fn[] = {
	__BPF_FUNC_MAPPER(BPF_HELPER_FN)
};
#undef BPF_HELPER_FN

static const char *reg(int r)
{
	static char	*name[] = { "%r0", "%r1", "%r2", "%r3", "%r4",
				    "%r5", "%r6", "%r7", "%r8", "%r9", "%fp" };

	return name[r];
}

static const char *
dt_dis_varname(const dtrace_difo_t *dp, uint_t id, uint_t scope)
{
	const dtrace_difv_t *dvp = dp->dtdo_vartab;
	uint_t i;

	id &= 0x0fff;
	for (i = 0; i < dp->dtdo_varlen; i++, dvp++) {
		if (dvp->dtdv_id == id && dvp->dtdv_scope == scope) {
			if (dvp->dtdv_name < dp->dtdo_strlen)
				return (dp->dtdo_strtab + dvp->dtdv_name);
			break;
		}
	}

	return (NULL);
}

static uint_t
dt_dis_scope(uint_t id)
{
	switch (id & 0x7000) {
	case 0x1000: return (DIFV_SCOPE_LOCAL);
	case 0x2000: return (DIFV_SCOPE_THREAD);
	case 0x3000: return (DIFV_SCOPE_GLOBAL);
	default: return (-1u);
	}
}

static const char *
dt_dis_scopename(uint_t scope)
{
	switch (scope) {
	case DIFV_SCOPE_LOCAL:
		return "this->";
	case DIFV_SCOPE_THREAD:
		return "self->";
	case DIFV_SCOPE_GLOBAL:
		return "";
	default:
		return "?->";
	}
}

/*ARGSUSED*/
static void
dt_dis_str(const dtrace_difo_t *dp, const char *name,
	   const struct bpf_insn *in, FILE *fp)
{
	fprintf(fp, "%s", name);
}

/*ARGSUSED*/
static void
dt_dis_op1(const dtrace_difo_t *dp, const char *name,
	   const struct bpf_insn *in, FILE *fp)
{
	fprintf(fp, "%-4s %s", name, reg(in->dst_reg));
}

/*ARGSUSED*/
static void
dt_dis_op2(const dtrace_difo_t *dp, const char *name,
	   const struct bpf_insn *in, FILE *fp)
{
	fprintf(fp, "%-4s %s, %s", name, reg(in->dst_reg), reg(in->src_reg));
}

/*ARGSUSED*/
static void
dt_dis_op2imm(const dtrace_difo_t *dp, const char *name,
	      const struct bpf_insn *in, FILE *fp)
{
	fprintf(fp, "%-4s %s, %d", name, reg(in->dst_reg), in->imm);
}

/*ARGSUSED*/
static void
dt_dis_branch(const dtrace_difo_t *dp, const char *name,
	      const struct bpf_insn *in, FILE *fp)
{
	fprintf(fp, "%-4s %s, %s, %d", name, reg(in->dst_reg),
		reg(in->src_reg), in->off);
}

/*ARGSUSED*/
static void
dt_dis_branch_imm(const dtrace_difo_t *dp, const char *name,
	      const struct bpf_insn *in, FILE *fp)
{
	fprintf(fp, "%-4s %s, %u, %d", name, reg(in->dst_reg), in->imm,
		in->off);
}

/*ARGSUSED*/
static void
dt_dis_load(const dtrace_difo_t *dp, const char *name,
	      const struct bpf_insn *in, FILE *fp)
{
	uint_t var = in->off;

	fprintf(fp, "%-4s %s, [%s%+d]", name, reg(in->dst_reg),
		reg(in->src_reg), in->off);

	if (in->src_reg == BPF_REG_FP) {
		uint_t		scope = dt_dis_scope(var);
		const char	*vname = dt_dis_varname(dp, var, scope);

		if (vname)
			fprintf(fp, "\t! %s%s",
				dt_dis_scopename(scope), vname);
	}
}

/*ARGSUSED*/
static void
dt_dis_load_imm(const dtrace_difo_t *dp, const char *name,
		const struct bpf_insn *in, FILE *fp)
{
	fprintf(fp, "%-4s %s, 0x%08x%08x", name, reg(in->dst_reg),
		in[1].imm, in[0].imm);
}

/*ARGSUSED*/
static void
dt_dis_store(const dtrace_difo_t *dp, const char *name,
	     const struct bpf_insn *in, FILE *fp)
{
	uint_t var = in->off;

	fprintf(fp, "%-4s [%s%+d], %s", name, reg(in->dst_reg), in->off,
		reg(in->src_reg));

	if (in->dst_reg == BPF_REG_FP) {
		uint_t		scope = dt_dis_scope(var);
		const char	*vname = dt_dis_varname(dp, var, scope);

		if (vname)
			fprintf(fp, "\t! %s%s",
				dt_dis_scopename(scope), vname);
	}
}

/*ARGSUSED*/
static void
dt_dis_store_imm(const dtrace_difo_t *dp, const char *name,
		 const struct bpf_insn *in, FILE *fp)
{
	fprintf(fp, "%-4s [%s%+d], %d", name, reg(in->dst_reg), in->off,
		in->imm);
}

static char *
dt_dis_bpf_args(const dtrace_difo_t *dp, uint_t id, const struct bpf_insn *in,
		char *buf, size_t len)
{
	char *s;

	switch (id) {
	case DT_BPF_GET_GVAR:
	case DT_BPF_SET_GVAR:
		/*
		 * We know that the previous instruction exists and assigns
		 * the variable id to %r1 (because we wrote the code generator
		 * to emit these instructions in this exact order.
		 */
		in--;
		snprintf(buf, len, "%s",
			 dt_dis_varname(dp, in->imm, DIFV_SCOPE_GLOBAL));
		return buf;
	case DT_BPF_GET_TVAR:
	case DT_BPF_SET_TVAR:
		/*
		 * We know that the previous instruction exists and assigns
		 * the variable id to %r1 (because we wrote the code generator
		 * to emit these instructions in this exact order.
		 */
		in--;
		snprintf(buf, len, "self->%s",
			 dt_dis_varname(dp, in->imm, DIFV_SCOPE_THREAD));
		return buf;
	case DT_BPF_GET_STRING:
		/*
		 * We know that the previous instruction exists and assigns
		 * the string offset to %r1 (because we wrote the code
		 * generator to emit these instructions in this exact order.
		 */
		in--;
		if (in->imm >= dp->dtdo_strlen)
			return NULL;

		s = dp->dtdo_strtab + in->imm;
		s = strchr2esc(s, strlen(s));
		snprintf(buf, len, "\"%s\"", s ? s : dp->dtdo_strtab + in->imm);
		free(s);
		return buf;
	default:
		return NULL;
	}
}

/*ARGSUSED*/
static void
dt_dis_call(const dtrace_difo_t *dp, const char *name,
	    const struct bpf_insn *in, FILE *fp)
{
	const char *fn = NULL;
	const char *ann = NULL;
	char buf[256];

	/*
	 * Try to determine a name for the function being called.  We also call
	 * a function-type (built-in BPF vs helper) specific function in case
	 * there are annotations to be added.
	 */
	if (in->src_reg == BPF_PSEUDO_CALL) {
		fn = dt_bpf_builtins[in->imm].name;
		ann = dt_dis_bpf_args(dp, in->imm, in, buf, sizeof(buf));
	} else if (in->imm >= 0 && in->imm < __BPF_FUNC_MAX_ID) {
		fn = helper_fn[in->imm];
	} else {
		snprintf(buf, sizeof(buf), "helper#%d", in->imm);
		fn = buf;
		ann = "unknown helper";
	}

	if (ann)
		fprintf(fp, "%-4s %-17s ! %s", name, fn, ann);
	else
		fprintf(fp, "%-4s %s", name, fn);
}

/*ARGSUSED*/
static void
dt_dis_jump(const dtrace_difo_t *dp, const char *name,
	    const struct bpf_insn *in, FILE *fp)
{
	if (in->off == 0)
		fprintf(fp, "nop");
	else
		fprintf(fp, "%-4s %u", name, in->off);
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
	default:
		snprintf(kind, sizeof (kind), "0x%x", t->dtdt_kind);
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
		snprintf(ckind, sizeof (ckind), "0x%x", t->dtdt_ckind);
	}

	if (t->dtdt_flags & DIF_TF_BYREF) {
		snprintf(buf, len, "%s (%s) by ref (size %lu)",
		    kind, ckind, (ulong_t)t->dtdt_size);
	} else {
		snprintf(buf, len, "%s (%s) (size %lu)",
		    kind, ckind, (ulong_t)t->dtdt_size);
	}

	return (buf);
}

static void
dt_dis_rtab(const char *rtag, const dtrace_difo_t *dp, FILE *fp,
    const dof_relodesc_t *rp, uint32_t len)
{
	fprintf(fp, "\n%-4s %-8s %-8s %s\n", rtag, "OFFSET", "DATA", "NAME");

	for (; len != 0; len--, rp++) {
		fprintf(fp, "%-4u %-8llu %-8llu %s\n", rp->dofr_type,
			(u_longlong_t)rp->dofr_offset,
			(u_longlong_t)rp->dofr_data,
			&dp->dtdo_strtab[rp->dofr_name]);
	}
}

static void
dt_dis_difo(const dtrace_difo_t *dp, FILE *fp)
{
	static const struct opent {
		const char *op_name;
		void (*op_func)(const dtrace_difo_t *, const char *,
				const struct bpf_insn *, FILE *);
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
		INSN3(JMP32, JSGT, K)	= { "jsgt", dt_dis_branch_imm },
		INSN3(JMP32, JSLT, K)	= { "jslt", dt_dis_branch_imm },
		INSN3(JMP32, JSGE, K)	= { "jsge", dt_dis_branch_imm },
		INSN3(JMP32, JSLE, K)	= { "jsle", dt_dis_branch_imm },
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
		INSN3(JMP, JSGT, K)	= { "jsgt", dt_dis_branch_imm },
		INSN3(JMP, JSLT, K)	= { "jslt", dt_dis_branch_imm },
		INSN3(JMP, JSGE, K)	= { "jsge", dt_dis_branch_imm },
		INSN3(JMP, JSLE, K)	= { "jsle", dt_dis_branch_imm },
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

	const struct opent *op;
	ulong_t i = 0;
	char type[DT_TYPE_NAMELEN];

	fprintf(fp, "%-3s %-4s %-20s    %s\n",
	    "INS", "OFF", "OPCODE", "INSTRUCTION");

	for (i = 0; i < dp->dtdo_len; i++) {
		const struct bpf_insn *instr = &dp->dtdo_buf[i];
		uint8_t opcode = instr->code;

		if (opcode >= sizeof (optab) / sizeof (optab[0]))
			opcode = 0; /* force invalid opcode message */

		fprintf(fp, "%03lu %03lu: %02x %01x %01x %04x %08x    ",
			i, i*8, instr->code, instr->dst_reg, instr->src_reg,
			instr->off, instr->imm);
		if (instr->code != 0) {
			op = &optab[opcode];
			op->op_func(dp, op->op_name, instr, fp);
		}
		fprintf(fp, "\n");
	}

	if (dp->dtdo_varlen != 0) {
		fprintf(fp, "\n%-16s %-4s %-3s %-3s %-4s %s\n",
			"NAME", "ID", "KND", "SCP", "FLAG", "TYPE");
	}

	for (i = 0; i < dp->dtdo_varlen; i++) {
		dtrace_difv_t *v = &dp->dtdo_vartab[i];
		char kind[4], scope[4], flags[16] = { 0 };

		switch (v->dtdv_kind) {
		case DIFV_KIND_ARRAY:
			strcpy(kind, "arr");
			break;
		case DIFV_KIND_SCALAR:
			strcpy(kind, "scl");
			break;
		default:
			snprintf(kind, sizeof (kind), "%u", v->dtdv_kind);
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
			snprintf(scope, sizeof (scope), "%u", v->dtdv_scope);
		}

		if (v->dtdv_flags & ~(DIFV_F_REF | DIFV_F_MOD)) {
			snprintf(flags, sizeof (flags), "/0x%x",
				 v->dtdv_flags & ~(DIFV_F_REF | DIFV_F_MOD));
		}

		if (v->dtdv_flags & DIFV_F_REF)
			strcat(flags, "/r");
		if (v->dtdv_flags & DIFV_F_MOD)
			strcat(flags, "/w");

		fprintf(fp, "%-16s %-4x %-3s %-3s %-4s %s\n",
			&dp->dtdo_strtab[v->dtdv_name],
			v->dtdv_id, kind, scope, flags + 1,
			dt_dis_typestr(&v->dtdv_type, type, sizeof (type)));
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
			dt_node_type_name(dnp, type, sizeof (type)));
	}

	if (dp->dtdo_krelen != 0)
		dt_dis_rtab("KREL", dp, fp, dp->dtdo_kreltab, dp->dtdo_krelen);

	if (dp->dtdo_urelen != 0)
		dt_dis_rtab("UREL", dp, fp, dp->dtdo_ureltab, dp->dtdo_urelen);
}

static void
dt_dis_format_print(const char *act_name, const char *fmt, FILE *fp)
{
	fprintf(fp, "%s(), format string %s", act_name, fmt);
	if (fmt[strlen (fmt)-1] != '\n')
		fprintf(fp,"\n");
}

static void
dt_dis_action(const dtrace_actdesc_t *ap, FILE *fp, const char *fmt)
{
	static const struct actent {
		long act;
		void (*act_print) (const char *act_name,
		    const char *fmt, FILE *fp);
		const char *act_name;
	} actab[] = {
		{ DTRACEACT_DIFEXPR, NULL, NULL },
		{ DTRACEACT_EXIT, NULL, "exit()" },
		{ DTRACEACT_PRINTF, dt_dis_format_print, "printf" },
		{ DTRACEACT_PRINTA, dt_dis_format_print, "printa" },
		{ DTRACEACT_LIBACT, NULL, "library-controlled" },
		{ DTRACEACT_USTACK, NULL, "ustack()" },
		{ DTRACEACT_JSTACK, NULL, "jstack()" },
		{ DTRACEACT_USYM, NULL, "usym()" },
		{ DTRACEACT_UMOD, NULL, "umod()" },
		{ DTRACEACT_UADDR, NULL, "uaddr()" },
		{ DTRACEACT_STOP, NULL, "stop()" },
		{ DTRACEACT_RAISE, NULL, "raise()" },
		{ DTRACEACT_SYSTEM, NULL, "system()" },
		{ DTRACEACT_FREOPEN, NULL, "freopen()" },
		{ DTRACEACT_STACK, NULL, "stack()" },
		{ DTRACEACT_SYM, NULL, "sym()" },
		{ DTRACEACT_MOD, NULL, "mod()" },
		{ DTRACEACT_BREAKPOINT, NULL, "breakpoint()" },
		{ DTRACEACT_PANIC, NULL, "panic()" },
		{ DTRACEACT_CHILL, NULL, "chill()" },
		{ DTRACEACT_SPECULATE, NULL, "speculate()" },
		{ DTRACEACT_COMMIT, NULL, "commit()" },
		{ DTRACEACT_DISCARD, NULL, "discard()" },
		{ 0, NULL }
	};

	dtrace_difo_t *dp = ap->dtad_difo;
	const struct actent *act = NULL;
	char type[DT_TYPE_NAMELEN];
	ulong_t i = 0;

	/*
	 * First, find the action (which may be NULL if it should not be
	 * mentioned in the output).
	 */
	for (i = 0; actab[i].act != 0; i++) {
		if (ap->dtad_kind == actab[i].act) {
			act = &actab[i];
			break;
		}
	}

	if ((act != NULL) && (act->act_name == NULL))
		act = NULL;

	/*
	 * There may be no DIFO at all, in which case just print the action.
	 * (If there is no DIFO nor action name, just print nothing.)
	 */
	if (dp == NULL) {
		if (act != NULL) {
			fprintf(fp,"\nAction, without DIFO, ");
			if (act->act_print != NULL)
				act->act_print(act->act_name, fmt, fp);
			else
				fprintf(fp, "%s\n", act->act_name);
		}
		return;
	}
	else if (act == NULL)
	{
		fprintf(fp, "\nDIFO %p returns %s\n", (void *)dp,
		    dt_dis_typestr(&dp->orig_dtdo_rtype, type, sizeof (type)));
	}
	else
	{
		fprintf(fp, "\nDIFO %p returning %s for action ", (void *)dp,
		    dt_dis_typestr(&dp->orig_dtdo_rtype, type, sizeof (type)));
		if (act->act_print != NULL)
			act->act_print(act->act_name, fmt, fp);
		else
			fprintf(fp, "%s\n", act->act_name);
	}
	dt_dis_difo(dp, fp);
}

static void
dt_dis_pred(const dtrace_preddesc_t *predp, FILE *fp)
{
	dtrace_difo_t *dp = predp->dtpdd_difo;
	char type[DT_TYPE_NAMELEN];

	if (dp == NULL)
		return;

	fprintf(fp, "\nPredicate DIFO %p returns %s\n", (void *)dp,
	    dt_dis_typestr(&dp->orig_dtdo_rtype, type, sizeof (type)));

	dt_dis_difo(dp, fp);
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
	dt_dis_iter_t *d = data;
	dtrace_preddesc_t *predp = &sdp->dtsd_ecbdesc->dted_pred;
	dtrace_actdesc_t *ap = sdp->dtsd_action;
	const char *fmt = NULL;

	if (predp == NULL && ap == NULL)
		return 0;

	if (d->last_ecb != sdp->dtsd_ecbdesc) {
		dtrace_probedesc_t *pdp = &sdp->dtsd_ecbdesc->dted_probe;

		fprintf(d->fp, "\nDisassembly of %s:%s:%s:%s\n",
			pdp->prv, pdp->mod, pdp->fun, pdp->prb);
		d->last_ecb = sdp->dtsd_ecbdesc;
	}

	if (sdp->dtsd_fmtdata != NULL) {
		dt_pfargv_t *pfv = sdp->dtsd_fmtdata;
		fmt = pfv->pfv_format;
	}

	dt_dis_pred(predp, d->fp);

	while (ap) {
		dt_dis_action(ap, d->fp, fmt);

		if (ap == sdp->dtsd_action_last)
			break;

		ap = ap->dtad_next;
	}

	return 0;
}

void
dt_dis_program(dtrace_hdl_t *dtp, dtrace_prog_t *pgp, FILE *fp)
{
	dt_dis_iter_t data = { fp };

	dtrace_stmt_iter(dtp, pgp, dt_dis_stmts, &data);
}
