/*
 * ISA-dependent dispatch code.
 */

/*
 * Oracle Linux DTrace.
 * Copyright (c) 2013, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <inttypes.h>
#include <errno.h>
#include <elf.h>
#include <sys/ptrace.h>

#include "Pcontrol.h"
#include "libproc.h"
#include "platform.h"

/*
 * Generate types for an ISA dispatch function.
 */
#define ISADEP_TYPES(ret, ...) \
	typedef ret dispatch_fun_t(__VA_ARGS__)

/*
 * Generate the body of an ISA dispatch function.
 */
#define ISADEP_BODY(ret, ...) do {					\
	dispatch_fun_t *dispatch_fun;					\
									\
	dispatch_fun = (dispatch_fun_t *)search_dispatch(P, dispatch);	\
    									\
	if (dispatch_fun == NULL) {					\
		_dprintf("%s: no ISA match for %s-bit process for ELF machine " \
		    "%i\n", __func__, P->elf64?"64":"32", P->elf_machine); \
		errno = ENOEXEC;					\
		return (ret)-1;						\
	}								\
									\
	return dispatch_fun(__VA_ARGS__);				\
	} while (0)

/*
 * General function dispatch table for ISA-specific functions.  A separate
 * instance of this (with different prototypes for func) exists for every
 * ISA-specific function.
 */

typedef void dispatch_fun_t(void);

typedef struct isa_dispatch {
	int elf64;
	int elf_machine;
	dispatch_fun_t *fun;
} isa_dispatch_t;

/*
 * Search a dispatch table.  The function pointer returned must be cast to the
 * correct prototype.
 */
static dispatch_fun_t *
search_dispatch(struct ps_prochandle *P, isa_dispatch_t *dispatch)
{
	isa_dispatch_t *search;

	for (search = dispatch; search->elf_machine != 0; search++) {
		if (search->elf64 == P->elf64 &&
		    search->elf_machine == P->elf_machine)
			return search->fun;
	}

	return NULL;
}

/*
 * Read the first argument of the function at which the process P is
 * halted, which must be a pointer.
 *
 * On error, -1 cast to a uintptr_t is returned, and errno is set.
 */
uintptr_t
Pread_first_arg(struct ps_prochandle *P)
{
#define WANT_FIRST_ARG_DISPATCH
#include "isadep.h"
#undef WANT_FIRST_ARG_DISPATCH

	ISADEP_TYPES(uintptr_t, struct ps_prochandle *);
	ISADEP_BODY(uintptr_t, P);
}

/*
 * Determine the instruction pointer address at which the process P is stopped.
 * An -ESRCH may be considered "acceptable fallout".
 *
 * Returns 0 (no breakpoint), an address, or -1 on error.  (We assume that an
 * address one from the top of the address space is an unlikely instruction
 * pointer value.)
 */
uintptr_t
Pget_bkpt_ip(struct ps_prochandle *P, int expect_esrch)
{
#define WANT_GET_BKPT_IP
#include "isadep.h"
#undef WANT_GET_BKPT_IP

	ISADEP_TYPES(uintptr_t, struct ps_prochandle *, int);
	ISADEP_BODY(uintptr_t, P, expect_esrch);
}

/*
 * Reset the instruction pointer address at which the process P is stopped.
 */
long
Preset_bkpt_ip(struct ps_prochandle *P, uintptr_t addr)
{
#define WANT_RESET_BKPT_IP
#include "isadep.h"
#undef WANT_RESET_BKPT_IP

	ISADEP_TYPES(long, struct ps_prochandle *, uintptr_t);
	ISADEP_BODY(long, P, addr);
}

#ifdef NEED_SOFTWARE_SINGLESTEP
/*
 * Get the next instruction pointer after this breakpoint.  Generally only
 * implemented (and only needed) on platforms without hardware singlestepping.
 */
uintptr_t
Pget_next_ip(struct ps_prochandle *P)
{
#define WANT_GET_NEXT_IP
#include "isadep.h"
#undef WANT_GET_NEXT_IP

	ISADEP_TYPES(uintptr_t, struct ps_prochandle *);
	ISADEP_BODY(uintptr_t, P);
}

#endif
