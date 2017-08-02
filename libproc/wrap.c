/*
 * Wrapping library.
 */

/*
 * Oracle Linux DTrace.
 * Copyright © 2013, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#define DO_NOT_POISON_PTRACE 1

#include <sys/ptrace.h>
#include <stdarg.h>
#include "Pcontrol.h"

/*
 * This file exists purely in order that wrapped functions can be poisoned
 * everywhere else.  No direct calls whatsoever to wrapped functions should exist
 * anywhere else.
 */

/*
 * Default (degenerate) ptrace() wrapper.
 */
static long
default_ptrace_wrapper(enum __ptrace_request request, void *arg, pid_t pid,
    void *addr, void *data)
{
	return ptrace(request, pid, addr, data);
}

/*
 * Call ptrace() using the wrapper.
 */
long
wrapped_ptrace(struct ps_prochandle *P, enum __ptrace_request request, pid_t pid,
    ...)
{
	va_list ap;
	void *addr;
	void *data;

	va_start(ap, pid);
	addr = va_arg(ap, void *);
	data = va_arg(ap, void *);
	va_end(ap);

	return P->ptrace_wrap(request, P->wrap_arg, pid, addr, data);
}

/*
 * Default (degenerate) Pwait() wrapper.
 */
static long
default_pwait_wrapper(struct ps_prochandle *P, void *arg, boolean_t block)
{
    return Pwait_internal(P, block);
}

/*
 * Call Pwait_internal() using the wrapper.
 */
long
Pwait(struct ps_prochandle *P, boolean_t block)
{
	return P->pwait_wrap(P, P->wrap_arg, block);
}

/*
 * Set the Ptrace() wrapper.
 */
void
Pset_ptrace_wrapper(struct ps_prochandle *P, ptrace_fun *wrapper)
{
	if (wrapper != NULL)
		P->ptrace_wrap = wrapper;
	else
		P->ptrace_wrap = default_ptrace_wrapper;
}

/*
 * Set the Pwait() wrapper.
 */
void
Pset_pwait_wrapper(struct ps_prochandle *P, pwait_fun *wrap)
{
	if (wrap != NULL)
		P->pwait_wrap = wrap;
	else
		P->pwait_wrap = default_pwait_wrapper;
}
