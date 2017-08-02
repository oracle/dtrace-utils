/*
 * ISA-dependent functions for SPARC and SPARC64.
 */

/*
 * Oracle Linux DTrace.
 * Copyright © 2013, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
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
 * which must be a pointer (though other integral arguments happen to work for
 * the SPARC and SPARC64 ABI).
 *
 * For SPARC, the first integral argument to a function will always be in %i0.
 * Horrifically, the value of all the UREG_* constants is off by 1, so we have
 * to correct for that.
 *
 * On error, -1 cast to a uintptr_t is returned, and errno is set.
 */
uintptr_t
Pread_first_arg_sparc64(struct ps_prochandle *P)
{
	struct pt_regs regs;

	if (getregs_sparc64(P, &regs) == NULL)
		return (uintptr_t) -1;

	return regs.u_regs[UREG_I0 - 1];
}

uintptr_t
Pread_first_arg_sparc32(struct ps_prochandle *P)
{
	struct pt_regs32 regs;

	if (getregs_sparc32(P, &regs) == NULL)
		return (uintptr_t) -1;

	return regs.u_regs[UREG_I0 - 1];
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
