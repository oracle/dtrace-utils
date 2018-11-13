/*
 * Oracle Linux DTrace.
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DTRACE_BPF_OPCODES_H
#define	_DTRACE_BPF_OPCODES_H

#include <linux/bpf.h>
#include <samples/bpf/bpf_insn.h>

#define BPF_NCLOBBERED	6
#define BPF_REG_FP	BPF_REG_10

#define BPF_NOP \
	BPF_MOV32_REG(0, 0)

#define FIRST_BPF_HELPER BPF_FUNC_dtrace_copys

static struct bpf_insn bpf_nop = BPF_NOP;

#define bpf_is_nop(insn) ((insn).code == bpf_nop.code &&	\
	    (insn).dst_reg == bpf_nop.dst_reg &&		\
	    (insn).src_reg == bpf_nop.src_reg &&		\
	    (insn).off == bpf_nop.off &&			\
	    (insn).imm == bpf_nop.imm)				\

#endif /* _DTRACE_BPF_OPCODES_H */
