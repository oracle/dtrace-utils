/*
 * ELF-related support code, bitness-independent.
 */

/*
 * Oracle Linux DTrace.
 * Copyright (c) 2013, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <sys/types.h>
#include <inttypes.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <elf.h>
#include <link.h>
#include <platform.h>

#ifndef NATIVE_BITNESS_ONLY
#define BITS 32
#include "elfish_impl.h"
#undef BITS
#endif
#define BITS 64
#include "elfish_impl.h"
#undef BITS

/*
 * Determine ISA-related info about the ELF file, and stash it in the
 * ps_prochandle.
 */
int
Pread_isa_info(struct ps_prochandle *P, const char *procname)
{
	int fd, err;
	Elf64_Ehdr hdr;

	if ((fd = open(procname, O_RDONLY)) < 0) {
		_dprintf("Cannot open %s: %s\n", procname, strerror(errno));
		return -1;
	}

	while ((err = read(fd, &hdr, sizeof(hdr))) < 0 && errno == EINTR);
	if (err < 0) {
		_dprintf("%s is not an ELF file\n", procname);
		close(fd);
		return -1;
	}
	close(fd);

	if (memcmp(&hdr, ELFMAG, SELFMAG) != 0) {
		_dprintf("%s is not an ELF file\n", procname);
		return -1;
	}

	P->elf64 = hdr.e_ident[EI_CLASS] == ELFCLASS64;
	P->elf_machine = hdr.e_machine;

#ifdef NATIVE_BITNESS_ONLY
	if (!P->elf64) {
		_dprintf("%s: cannot trace 32-bit processes on this platform.\n",
		    procname);
		return -1;
	}
#endif
	return 0;
}

/*
 * Re-read the /proc/<pid>/auxv file and store its contents in P->auxv.
 */
void
Preadauxvec(struct ps_prochandle *P)
{
	int auxv_size = P->elf64 ? sizeof(Elf64_auxv_t) : sizeof(Elf32_auxv_t);
	char auxfile[PATH_MAX + MAXLEN_PID + strlen("/auxv") + 1];
	ssize_t i;
	int fd;
	char buf[sizeof(Elf64_auxv_t)];

	if (P->state == PS_DEAD)
		return;

	free(P->auxv);
	P->auxv = NULL;
	P->nauxv = 0;

	snprintf(auxfile, sizeof(auxfile), "%s/%d/auxv", procfs_path,
	    (int)P->pid);
	if ((fd = open(auxfile, O_RDONLY)) < 0) {
		_dprintf("Cannot open auxiliary vector file %s: %s\n",
		    auxfile, strerror(errno));
		return;
	}

	/*
	 * First, count the number of entries in auxv.
	 *
	 * We assume no short or interrupted reads in /proc (if < page size,
	 * anyway).
	 */
	errno = 0;
	for (P->nauxv = 0; read(fd, buf, auxv_size) == auxv_size; P->nauxv++);

	if ((errno != 0) || lseek(fd, 0, SEEK_SET) == -1) {
		_dprintf("Error reading from auxv: %s\n", strerror(errno));
		close(fd);
		return;
	}
	_dprintf("%i: %u auxv entries.\n", P->pid, P->nauxv);

	if ((P->auxv = malloc(P->nauxv * sizeof(auxv_t))) == NULL) {
		_dprintf("Out of memory allocating aux vector\n");
		close(fd);
		return;
	}

	for (i = 0; i < P->nauxv && read(fd, buf, auxv_size) == auxv_size;
	     i++) {
		if (P->elf64) {
			Elf64_auxv_t *auxv = (Elf64_auxv_t *)buf;

			P->auxv[i].a_type = auxv->a_type;
			P->auxv[i].a_un.a_val = auxv->a_un.a_val;
		} else {
			Elf32_auxv_t *auxv = (Elf32_auxv_t *)buf;

			P->auxv[i].a_type = auxv->a_type;
			P->auxv[i].a_un.a_val = auxv->a_un.a_val;
		}
	}

	close(fd);
}

/*
 * Return a requested element from the process's aux vector.
 * Return -1 on failure (this is adequate for our purposes).
 */
uint64_t
Pgetauxval(struct ps_prochandle *P, int type)
{
	auxv_t *auxv;
	ssize_t nauxv;

	if (Pstate(P) == PS_DEAD)
		return -1;

	if (P->auxv == NULL)
		Preadauxvec(P);

	if (P->auxv == NULL)
		return -1;

	for (auxv = P->auxv, nauxv = P->nauxv;
	     nauxv > 0 && auxv->a_type != AT_NULL;
	     auxv++, nauxv--) {
		if (auxv->a_type == type)
			return auxv->a_un.a_val;
	}

	return -1;
}

uintptr_t
r_debug(struct ps_prochandle *P)
{
	if (Pstate(P) == PS_DEAD)
		return 0;

	if (P->r_debug_addr)
		return P->r_debug_addr;

	if (P->elf64)
		P->r_debug_addr = r_debug_elf64(P);
#ifndef NATIVE_BITNESS_ONLY
	else
		P->r_debug_addr = r_debug_elf32(P);
#endif

	return P->r_debug_addr;
}
