/*
 * ISA-dependent functions for x86 and x86-64.
 */

/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2013 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <inttypes.h>
#include <errno.h>
#include <sys/reg.h>

#include "Pcontrol.h"
#include "libproc.h"

/*
 * Read the first argument of the function at which the process P is halted,
 * which must be a pointer (thuogh other integral arguments happen to work for
 * the x86-64 ABI).
 *
 * On error, -1 cast to a uintptr_t is returned, and errno is set.
 */

uintptr_t
Pread_first_arg_x86_64(struct ps_prochandle *P)
{
	long addr;

	if (Pstate(P) == PS_RUN ||
	    Pstate(P) == PS_DEAD)
		return (uintptr_t) -1;

	/*
	 * For x86-64, the first integral argument to a function will always be
	 * in %rdi.
	 */

	addr = wrapped_ptrace(P, PTRACE_PEEKUSER, P->pid, RDI * sizeof (long));

	if ((errno == ESRCH) || (addr < 0))
		return (uintptr_t) -1;

	return addr;
}

uintptr_t
Pread_first_arg_x86(struct ps_prochandle *P)
{
	uintptr_t stackaddr;
	long addr;

	if (Pstate(P) == PS_RUN ||
	    Pstate(P) == PS_DEAD)
		return (uintptr_t) -1;

	/*
	 * For 32-bit x86, we have to look on the stack.
	 */

	stackaddr = wrapped_ptrace(P, PTRACE_PEEKUSER, P->pid,
	    RSP * sizeof (long));

	if ((errno == ESRCH) || (stackaddr < 0))
		return (uintptr_t) -1;

	stackaddr += 4; /* 32-bit Linux x86's sizeof (long), stack grows down */
	if (Pread_scalar(P, &addr, 4, sizeof(addr), stackaddr) < 0)
		return (uintptr_t) -1;

	return addr;
}
