/*
 * ISA-dependent dispatch code.
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
#include <elf.h>

#include "Pcontrol.h"
#include "libproc.h"
#include "isadep_impl.h"

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
 * Quash object-pointer conversion warnings.
 *
 * TODO: When we can rely on a GCC version supporting #pragma diagnostic
 * push/pop, use them.
 */
#pragma GCC diagnostic ignored "-pedantic"

/*
 * Read the first argument of the function at which the process P is
 * halted, which must be a pointer.
 *
 * On error, -1 cast to a uintptr_t is returned, and errno is set.
 */
uintptr_t
Pread_first_arg(struct ps_prochandle *P)
{
	isa_dispatch_t dispatch[] = {
		{B_TRUE, EM_X86_64, (dispatch_fun_t *) Pread_first_arg_x86_64},
		{B_FALSE, EM_386, (dispatch_fun_t *) Pread_first_arg_x86},
		{0, 0, NULL} };

	typedef uintptr_t read_first_arg_fun_t(struct ps_prochandle *P);
	read_first_arg_fun_t *read_first_arg_fun;

	read_first_arg_fun = (read_first_arg_fun_t *) search_dispatch(P,
	    dispatch);

	if (read_first_arg_fun == NULL) {
		_dprintf("No ISA match for %s-bit process for ELF machine %i "
		    "while reading first arg\n", P->elf64?"64":"32",
		    P->elf_machine);
		errno = ENOEXEC;
		return (uintptr_t) -1;
	}

	return read_first_arg_fun(P);
}
