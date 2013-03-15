/*
 * ELF-related support code, bitness-dependent (64-bit by default).
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
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <elf.h>

#include "Pcontrol.h"
#include "libproc.h"

#ifndef BITS
#define BITS 64
#endif

#include "elfish_impl.h"

#ifndef BITIZE
#define JOIN(pre,post) pre##_elf##post
#define EXJOIN(pre,post) JOIN(pre,post)
#define JOINMID(pre,mid,post) pre##mid##post
#define EXJOINMID(pre,mid,post) JOINMID(pre,mid,post)
#define BITIZE(pre) EXJOIN(pre,BITS)
#define ElfIZE(suffix) EXJOINMID(Elf,BITS,_##suffix)
#define ELFIZE(suffix) EXJOINMID(ELF,BITS,_##suffix)
#endif

/*
 * Find the r_debug structure for the base lmid in a running, traced process.
 * Return -1 if none, or if the process is not running.
 *
 * r_debugs for lmids other than LM_ID_BASE require a different method:
 * see rtld_db.c:ns_debug_addr().
 */
uintptr_t
BITIZE(r_debug)(struct ps_prochandle *P)
{
	uint64_t phaddr = Pgetauxval(P, AT_PHDR);
	uint64_t phent = Pgetauxval(P, AT_PHENT);
	uint64_t phnum = Pgetauxval(P, AT_PHNUM);
	uintptr_t reloc = 0;
	uint64_t num_found = 0;
	ElfIZE(Phdr) phdr;

	ElfIZE(Addr) dynaddr = 0;
	ElfIZE(Xword) dynsize = 0;
	ElfIZE(Dyn) dyn;

	uint64_t i;

	if ((phaddr == -1) || (phent == -1) || (phnum == -1)) {
		fprintf(stderr, "%i: no phaddr, phent or phnum auxvec "
		    "entry.\n", P->pid);
		return -1;
	}

	if (P->state == PS_DEAD) {
		fprintf(stderr, "%i: process is dead.", P->pid);
		return -1;
	}

	if (phent != sizeof (phdr)) {
		static int warned = 0;
		if (!warned) {
			fprintf(stderr, "%i: AT_PHENT is the wrong "
			    "size: %li bytes versus an expected %li.\n"
			    "This should never happen.\n", P->pid, phent,
			    sizeof (phdr));
			warned = 1;
		}
		return -1;
	}

	/*
	 * Find the PT_DYNAMIC and PT_PHDR sections.
	 */
	for (i = 0; i < phnum && num_found < 2; i++, phaddr += sizeof (phdr)) {
		ssize_t foo;

		if ((foo = Pread(P, &phdr, sizeof (phdr),
			    (uintptr_t) phaddr)) != sizeof (phdr)) {

			_dprintf("short read: phdr end: read %li, sizeof (phdr) %li\n",
			    foo, sizeof (phdr));
			/*
			 * Short read -> assume end of phdr.
			 */
			break;
		}

		if (phdr.p_type == PT_DYNAMIC) {
			dynaddr = phdr.p_vaddr;
			dynsize = phdr.p_memsz;
			num_found++;
		} else if (phdr.p_type == PT_PHDR) {
			/*
			 * If this exists, we can use it to figure out the
			 * relocation, if any, which should be applied to
			 * find the dynamic section.  For PIE executables, this
			 * can be nonzero.
			 */
			reloc = phaddr - phdr.p_vaddr;
			num_found++;
		}
	}

	if (!dynaddr) {
		/*
		 * No PT_DYNAMIC: probably statically linked.
		 *
		 * Look for the symbol by name.
		 */
		GElf_Sym sym;

		P->no_dyn = TRUE;
		if (Pxlookup_by_name(P, PR_LMID_EVERY, PR_OBJ_EVERY,
			"_r_debug", &sym, NULL) < 0) {
			_dprintf("%i: cannot find r_debug: no dynaddr.\n", P->pid);
			return 0;
		} else
			return sym.st_value;
	}

	/*
	 * Find the DT_DEBUG dynamic tag.
	 */

	for (i = 0; i < dynsize; i += sizeof (dyn), dynaddr += sizeof (dyn)) {
		if (Pread(P, &dyn, sizeof (dyn),
			(uintptr_t) (dynaddr + reloc)) != sizeof (dyn)) {
			/*
			 * Short read -> assume end of dynamic section.
			 */
			_dprintf("%i: cannot find r_debug: short read in "
			    "dynamic section.\n", P->pid);
			return -1;
		}

		if (dyn.d_tag == DT_DEBUG)
			return dyn.d_un.d_ptr;
	}

	_dprintf("%i: cannot find r_debug: no DT_DEBUG dynamic tag.\n", P->pid);
	return -1;
}
#undef JOIN
#undef EXJOIN
#undef JOINMID
#undef EXJOINMID
#undef BITIZE
#undef ElfIZE
#undef ELFIZE
