/*
 * ISA-dependent functions for x86 and x86-64.
 */

/*
 * Oracle Linux DTrace.
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <inttypes.h>
#include <errno.h>
#include <string.h>
#include <sys/ptrace.h>

#include "Pcontrol.h"
#include "libproc.h"
#include "platform.h"

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

/*
 * The kernel translates between 32- and 64-bit regsets for us, but does not
 * helpfully adjust for the fact that the trap address needs adjustment on some
 * platforms before it will correspond to the address of the breakpoint.
 */
uintptr_t
Pget_bkpt_ip_x86(struct ps_prochandle *P, int expect_esrch)
{
	long ip;
	errno = 0;
	ip = wrapped_ptrace(P, PTRACE_PEEKUSER, P->pid, RIP * sizeof (long));
	if ((errno == ESRCH) && (expect_esrch))
	    return(0);

	if (errno != 0) {
		_dprintf("Unexpected ptrace (PTRACE_PEEKUSER) error: %s\n",
		    strerror(errno));
		return(-1);
	}
	/*
	 * The x86 increments its instruction pointer before triggering the
	 * trap, so we must undo it again.
	 */
	ip -= 1;

	return ip;
}

/*
 * Reset the instruction pointer address at which the process P is stopped.
 * (Only used to reset the instruction pointer to compensate for
 * platform-specific IP adjustment in Pget_bkpt_ip_x86().)
 */
long
Preset_bkpt_ip_x86(struct ps_prochandle *P, uintptr_t addr)
{
	return wrapped_ptrace(P, PTRACE_POKEUSER, P->pid, RIP * sizeof (long),
	    addr);
}
