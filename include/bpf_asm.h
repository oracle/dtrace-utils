/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef BPF_ASM_H
#define BPF_ASM_H

#include <linux/bpf.h>

#ifndef BPF_JMP32
# define BPF_JMP32	0x06
#endif
#define BPF_REG_FP	BPF_REG_10

#define BPF_ALU64_REG(op, dst, src)					\
	((struct bpf_insn) {						\
		.code = BPF_ALU64 | op | BPF_X,				\
		.dst_reg = dst,						\
		.src_reg = src,						\
		.off = 0,						\
		.imm = 0						\
	})

#define BPF_NEG_REG(dst)						\
	((struct bpf_insn) {						\
		.code = BPF_ALU64 | BPF_NEG,				\
		.dst_reg = dst,						\
		.src_reg = 0,						\
		.off = 0,						\
		.imm = 0						\
	})

#define BPF_ALU64_IMM(op, dst, val)					\
	((struct bpf_insn) {						\
		.code = BPF_ALU64 | op | BPF_K,				\
		.dst_reg = dst,						\
		.src_reg = 0,						\
		.off = 0,						\
		.imm = (val)						\
	})

#define BPF_MOV_REG(dst, src)	BPF_ALU64_REG(BPF_MOV, dst, src)
#define BPF_MOV_IMM(dst, val)	BPF_ALU64_IMM(BPF_MOV, dst, val)

#define BPF_LOAD(sz, dst, src, ofs)					\
	((struct bpf_insn) {						\
		.code = BPF_LDX | BPF_MEM | sz,				\
		.dst_reg = dst,						\
		.src_reg = src,						\
		.off = (ofs),						\
		.imm = 0						\
	})

#define BPF_LDDW(dst, val)						\
	((struct bpf_insn) {						\
		.code = BPF_LD | BPF_IMM | BPF_DW,			\
		.dst_reg = dst,						\
		.src_reg = 0,						\
		.off = 0,						\
		.imm = (uint32_t)(val)					\
	}),								\
	((struct bpf_insn) {						\
		.code = 0,						\
		.dst_reg = 0,						\
		.src_reg = 0,						\
		.off = 0,						\
		.imm = (uint32_t)((uint64_t)(val) >> 32)		\
	})

#define BPF_STORE(sz, dst, ofs, src)					\
	((struct bpf_insn) {						\
		.code = BPF_ST | BPF_MEM | sz,				\
		.dst_reg = dst,						\
		.src_reg = src,						\
		.off = (ofs),						\
		.imm = 0						\
	})

#define BPF_STORE_IMM(sz, dst, ofs, val)				\
	((struct bpf_insn) {						\
		.code = BPF_ST | BPF_IMM | sz,				\
		.dst_reg = dst,						\
		.src_reg = 0,						\
		.off = (ofs),						\
		.imm = (val)						\
	})

#define BPF_CALL_HELPER(id)						\
	((struct bpf_insn) {						\
		.code = BPF_JMP | BPF_CALL,				\
		.dst_reg = 0,						\
		.src_reg = 0,						\
		.off = 0,						\
		.imm = (id)						\
	})

#define BPF_CALL_FUNC(val)						\
	((struct bpf_insn) {						\
		.code = BPF_JMP | BPF_CALL,				\
		.dst_reg = 0,						\
		.src_reg = BPF_PSEUDO_CALL,				\
		.off = 0,						\
		.imm = (val)						\
	})

#define BPF_RETURN()							\
	((struct bpf_insn) {						\
		.code = BPF_JMP | BPF_EXIT,				\
		.dst_reg = 0,						\
		.src_reg = 0,						\
		.off = 0,						\
		.imm = 0						\
	})

#define BPF_BRANCH_REG(op, r1, r2, ofs)					\
	((struct bpf_insn) {						\
		.code = BPF_JMP | op | BPF_X,				\
		.dst_reg = r1,						\
		.src_reg = r2,						\
		.off = (ofs),						\
		.imm = 0						\
	})

#define BPF_BRANCH_IMM(op, r1, val, ofs)				\
	((struct bpf_insn) {						\
		.code = BPF_JMP | op | BPF_K,				\
		.dst_reg = r1,						\
		.src_reg = 0,						\
		.off = (ofs),						\
		.imm = (val)						\
	})

#define BPF_JUMP(ofs)							\
	((struct bpf_insn) {						\
		.code = BPF_JMP | BPF_JA,				\
		.dst_reg = 0,						\
		.src_reg = 0,						\
		.off = (ofs),						\
		.imm = 0						\
	})

#define BPF_NOP()	BPF_JUMP(0)

#define BPF_IS_NOP(x)	((x).code == (BPF_JMP | BPF_JA) && (x).off == 0)
#define BPF_EQUAL(x, y)	((x).code == (y).code &&			\
			 (x).dst_reg == (y).dst_reg &&			\
			 (x).src_reg == (y).src_reg &&			\
			 (x).off == (y).off &&				\
			 (x).imm == (y).imm)

#endif /* BPF_ASM_H */
