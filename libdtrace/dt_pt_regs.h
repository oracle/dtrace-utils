/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, 2022, Oracle and/or its affiliates. All rights reserved.
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
# define PT_REGS_RET		offsetof(dt_pt_regs, rax)
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
#elif defined(__aarch64__)
typedef struct user_pt_regs	dt_pt_regs;
# define PT_REGS_RET		offsetof(dt_pt_regs, regs[0])
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
#else
# error ISA not supported
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_PR_REGS_H */
