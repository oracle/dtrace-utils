/*
 * ISA-dependent functions for ARM64.
 */

/*
 * Oracle Linux DTrace.
 * Copyright (c) 2013, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <inttypes.h>
#include <errno.h>
#include <string.h>
#include <sys/ptrace.h>
#include <asm/ptrace.h>

#include "Pcontrol.h"
#include "libproc.h"
#include "platform.h"

/*
 * Get an ARM64 regset.
 */
static int
getregs_arm64(struct ps_prochandle *P, struct user_pt_regs *regs)
{
	struct iovec iov;

	iov.iov_base = regs;
	iov.iov_len = sizeof(struct user_pt_regs);

	if (Pstate(P) == PS_RUN ||
	    Pstate(P) == PS_DEAD)
		return -1;

	if (wrapped_ptrace(P, PTRACE_GETREGSET, P->pid, NT_PRSTATUS, &iov) < 0)
		return -1;

	return 0;
}

/*
 * Read the first argument of the function at which the process P is halted,
 * which must be a pointer (thuogh other integral arguments happen to work for
 * the x86-64 ABI).
 *
 * On error, -1 cast to a uintptr_t is returned, and errno is set.
 */

uintptr_t
Pread_first_arg_arm64(struct ps_prochandle *P)
{
	long addr;
	struct user_pt_regs regs;

	/*
	 * The argument is in %x0, general-purpose register 0.
	 */

	if (getregs_arm64(P, &regs) < 0)
		return (uintptr_t)-1;

	addr = regs.regs[0];

	if ((errno == ESRCH) || (addr < 0))
		return (uintptr_t)-1;

	return addr;
}

/*
 * The kernel translates between 32- and 64-bit regsets for us, but does not
 * helpfully adjust for the fact that the trap address needs adjustment on some
 * platforms before it will correspond to the address of the breakpoint.
 */
uintptr_t
Pget_bkpt_ip_arm64(struct ps_prochandle *P, int expect_esrch)
{
	long ip;
	errno = 0;
	struct user_pt_regs regs;

	if (getregs_arm64(P, &regs) < 0)
		return (uintptr_t)-1;
	ip = regs.pc;

	if ((errno == ESRCH) && (expect_esrch))
	    return 0;

	if (errno != 0) {
		_dprintf("Unexpected ptrace (PTRACE_GETREGSET) error: %s\n",
		    strerror(errno));
		return -1;
	}

	return ip;
}

/*
 * Reset the instruction pointer address at which the process P is stopped.
 * (Not needed on ARM.)
 */
long
Preset_bkpt_ip_arm64(struct ps_prochandle *P, uintptr_t addr)
{
    return 0; /* nothing doing */
}
