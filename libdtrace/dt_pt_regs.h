/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_PR_REGS_H
#define	_DT_PR_REGS_H

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

# define PT_REGS_BPF_ARG0(r)	((r)->rdi)
# define PT_REGS_BPF_ARG1(r)	((r)->rsi)
# define PT_REGS_BPF_ARG2(r)	((r)->rdx)
# define PT_REGS_BPF_ARG3(r)	((r)->rcx)
# define PT_REGS_BPF_ARG4(r)	((r)->r8)
# define PT_REGS_BPF_ARG5(r)	((r)->r9)
#elif defined(__aarch64__)
typedef struct user_pt_regs	dt_pt_regs;
# define PT_REGS_ARG0		offsetof(dt_pt_regs, regs[0])
# define PT_REGS_ARG1		offsetof(dt_pt_regs, regs[1])
# define PT_REGS_ARG2		offsetof(dt_pt_regs, regs[2])
# define PT_REGS_ARG3		offsetof(dt_pt_regs, regs[3])
# define PT_REGS_ARG4		offsetof(dt_pt_regs, regs[4])
# define PT_REGS_ARG5		offsetof(dt_pt_regs, regs[5])

# define PT_REGS_BPF_ARG0(r)	((r)->regs[0])
# define PT_REGS_BPF_ARG1(r)	((r)->regs[1])
# define PT_REGS_BPF_ARG2(r)	((r)->regs[2])
# define PT_REGS_BPF_ARG3(r)	((r)->regs[3])
# define PT_REGS_BPF_ARG4(r)	((r)->regs[4])
# define PT_REGS_BPF_ARG5(r)	((r)->regs[5])
#else
# error ISA not supported
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_PR_REGS_H */
