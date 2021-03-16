/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_PR_REGS_H
#define	_DT_PR_REGS_H

#ifndef __BPF__
# include <sys/ptrace.h>
#endif
#include <asm/ptrace.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(__amd64)
typedef struct pt_regs		dt_pt_regs;

# define PT_REGS_ARG0		offsetof(dt_pt_regs, rdi)
# define PT_REGS_ARG1		offsetof(dt_pt_regs, rsi)
# define PT_REGS_ARG2		offsetof(dt_pt_regs, rdx)
# define PT_REGS_ARG3		offsetof(dt_pt_regs, rcx)
# define PT_REGS_ARG4		offsetof(dt_pt_regs, r8)
# define PT_REGS_ARG5		offsetof(dt_pt_regs, r9)
# define PT_REGS_ARGC		6
# define PT_REGS_ARGSTKBASE	1
# define PT_REGS_IP		offsetof(dt_pt_regs, rip)
# define PT_REGS_SP		offsetof(dt_pt_regs, rsp)

# define PT_REGS_BPF_ARG0(r)	((r)->rdi)
# define PT_REGS_BPF_ARG1(r)	((r)->rsi)
# define PT_REGS_BPF_ARG2(r)	((r)->rdx)
# define PT_REGS_BPF_ARG3(r)	((r)->rcx)
# define PT_REGS_BPF_ARG4(r)	((r)->r8)
# define PT_REGS_BPF_ARG5(r)	((r)->r9)
# define PT_REGS_BPF_IP(r)	((r)->rip)
# define PT_REGS_BPF_SP(r)	((r)->rsp)
#elif defined(__aarch64__)
typedef struct user_pt_regs	dt_pt_regs;
# define PT_REGS_ARG0		offsetof(dt_pt_regs, regs[0])
# define PT_REGS_ARG1		offsetof(dt_pt_regs, regs[1])
# define PT_REGS_ARG2		offsetof(dt_pt_regs, regs[2])
# define PT_REGS_ARG3		offsetof(dt_pt_regs, regs[3])
# define PT_REGS_ARG4		offsetof(dt_pt_regs, regs[4])
# define PT_REGS_ARG5		offsetof(dt_pt_regs, regs[5])
# define PT_REGS_ARG6		offsetof(dt_pt_regs, regs[6])
# define PT_REGS_ARG7		offsetof(dt_pt_regs, regs[7])
# define PT_REGS_ARGC		8
# define PT_REGS_ARGSTKBASE	0
# define PT_REGS_IP		offsetof(dt_pt_regs, pc)
# define PT_REGS_SP		offsetof(dt_pt_regs, sp)

# define PT_REGS_BPF_ARG0(r)	((r)->regs[0])
# define PT_REGS_BPF_ARG1(r)	((r)->regs[1])
# define PT_REGS_BPF_ARG2(r)	((r)->regs[2])
# define PT_REGS_BPF_ARG3(r)	((r)->regs[3])
# define PT_REGS_BPF_ARG4(r)	((r)->regs[4])
# define PT_REGS_BPF_ARG5(r)	((r)->regs[5])
# define PT_REGS_BPF_ARG6(r)	((r)->regs[6])
# define PT_REGS_BPF_ARG7(r)	((r)->regs[7])
# define PT_REGS_BPF_IP(r)	((r)->pc)
# define PT_REGS_BPF_SP(r)	((r)->sp)
#else
# error ISA not supported
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_PR_REGS_H */
