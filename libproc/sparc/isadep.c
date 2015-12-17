/*
 * ISA-dependent functions for SPARC and SPARC64.
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
#include <string.h>
#include <sys/ptrace.h>
#include <asm/ptrace.h>				/* for struct pt_regs */

#include "Pcontrol.h"
#include "libproc.h"
#include "platform.h"

#define PTRACE_GETREGSET        0x4204

/*
 * Get a SPARC64 regset.
 */
static struct pt_regs *
getregs_sparc64(struct ps_prochandle *P, struct pt_regs *regs)
{
	if (Pstate(P) == PS_RUN ||
	    Pstate(P) == PS_DEAD)
		return NULL;

	if (wrapped_ptrace(P, PTRACE_GETREGS64, P->pid, regs) < 0)
		return NULL;

	return regs;
}

static struct pt_regs32 *
getregs_sparc32(struct ps_prochandle *P, struct pt_regs32 *regs)
{
	if (Pstate(P) == PS_RUN ||
	    Pstate(P) == PS_DEAD)
		return NULL;

	if (wrapped_ptrace(P, PTRACE_GETREGS, P->pid, regs) < 0)
		return NULL;

	return regs;
}

/*
 * Read the first argument of the function at which the process P is halted,
 * which must be a pointer (thuogh other integral arguments happen to work for
 * the SPARC and SPARC64 ABI).
 *
 * For SPARC, the first integral argument to a function will always be
 * in %i0.
 *
 * On error, -1 cast to a uintptr_t is returned, and errno is set.
 */
uintptr_t
Pread_first_arg_sparc64(struct ps_prochandle *P)
{
	struct pt_regs regs;

	if (getregs_sparc64(P, &regs) == NULL)
		return (uintptr_t) -1;

	return regs.u_regs[UREG_I0];
}

uintptr_t
Pread_first_arg_sparc32(struct ps_prochandle *P)
{
	struct pt_regs32 regs;

	if (getregs_sparc32(P, &regs) == NULL)
		return (uintptr_t) -1;

	return regs.u_regs[UREG_I0];
}

/*
 * Get the address at which we stopped at a breakpoint.  Unlike on x86, no post
 * facto adjustment is required.
 */
uintptr_t
Pget_bkpt_ip_sparc64(struct ps_prochandle *P, int expect_esrch)
{
	struct pt_regs regs;

	if (getregs_sparc64(P, &regs) == NULL) {
		if ((errno == ESRCH) && (expect_esrch))
			return(0);
		else {
			_dprintf("Unexpected ptrace (PTRACE_GETREGSET) "
			    "error: %s\n", strerror(errno));
			return(-1);
		}
	}

	return regs.tpc;
}

/*
 * Reset the instruction pointer address at which the process P is stopped.
 * (Not needed on SPARC.)
 */
long
Preset_bkpt_ip_sparc(struct ps_prochandle *P, uintptr_t addr)
{
    return 0; /* nothing doing */
}

/*
 * Get the address at which the temporary breakpoint should be dropped when a
 * breakpoint is hit.
 */
extern long Pget_next_ip_sparc64(struct ps_prochandle *P, uintptr_t addr)
{
	struct pt_regs regs;

	if (getregs_sparc64(P, &regs) == NULL) {
		_dprintf("Unexpected ptrace (PTRACE_GETREGSET) "
		    "error: %s\n", strerror(errno));
		return(-1);
	}

	return regs.tnpc;
}
