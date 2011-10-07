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
 * Copyright 2009 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <sys/stack.h>
#include <sys/sysmacros.h>
#include <sys/machelf.h>

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>

#include "Pcontrol.h"

const char *
Ppltdest(struct ps_prochandle *P, uintptr_t pltaddr)
{
	map_info_t *mp = Paddr2mptr(P, pltaddr);
	file_info_t *fp;
	size_t i;
	uintptr_t r_addr;
#ifdef USERSPACE_TRACEPOINTS
	char dmodel = Pstatus(P)->pr_dmodel;
#else
	char dmodel =  PR_MODEL_LP64;
#endif

	if (mp == NULL || (fp = mp->map_file) == NULL ||
	    fp->file_plt_base == 0 ||
	    pltaddr - fp->file_plt_base >= fp->file_plt_size) {
		errno = EINVAL;
		return (NULL);
	}

	i = (pltaddr - fp->file_plt_base) / M_PLT_ENTSIZE - M_PLT_XNumber;

#ifdef USERSPACE_TRACEPOINTS
	if (P->status.pr_dmodel == PR_MODEL_LP64) {
#else
	if (dmodel == PR_MODEL_LP64) {
#endif
		Elf64_Rela r;

		r_addr = fp->file_jmp_rel + i * sizeof (r);

		if (Pread(P, &r, sizeof (r), r_addr) == sizeof (r) &&
		    (i = ELF64_R_SYM(r.r_info)) < fp->file_dynsym.sym_symn) {
			Elf_Data *data = fp->file_dynsym.sym_data_pri;
			Elf64_Sym *symp = &(((Elf64_Sym *)data->d_buf)[i]);

			return (fp->file_dynsym.sym_strs + symp->st_name);
		}
	} else {
		Elf32_Rel r;

		r_addr = fp->file_jmp_rel + i * sizeof (r);

		if (Pread(P, &r, sizeof (r), r_addr) == sizeof (r) &&
		    (i = ELF32_R_SYM(r.r_info)) < fp->file_dynsym.sym_symn) {
			Elf_Data *data = fp->file_dynsym.sym_data_pri;
			Elf32_Sym *symp = &(((Elf32_Sym *)data->d_buf)[i]);

			return (fp->file_dynsym.sym_strs + symp->st_name);
		}
	}

	return (NULL);
}
