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
 * Copyright 2008, 2011 -- 2013 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_PCONTROL_H
#define	_PCONTROL_H

/*
 * Implemention-specific include file for libproc process management.
 * This is not to be seen by the clients of libproc.
 */

#include <stdio.h>
#include <gelf.h>
#include <rtld_db.h>
#include <libproc.h>
#include <sys/ctf_api.h>
#include <limits.h>

#include <mutex.h>

#ifdef	__cplusplus
extern "C" {
#endif

#include "Putil.h"

/*
 * Definitions of the process control structures, internal to libproc.
 * These may change without affecting clients of libproc.
 */

/*
 * sym_tbl_t contains a primary and an (optional) auxiliary symbol table, which
 * we wish to treat as a single logical symbol table. In this logical table,
 * the data from the auxiliary table preceeds that from the primary. Symbol
 * indices start at [0], which is the first item in the auxiliary table
 * if there is one. The sole purpose for this is so that we can treat the
 * combination of .SUNW_ldynsym and .dynsym sections as a logically single
 * entity without having to violate the public interface to libelf.
 *
 * Both tables must share the same string table section.
 *
 * The symtab_getsym() function serves as a gelf_getsym() replacement
 * that is aware of the two tables and makes them look like a single table
 * to the caller.
 *
 */
typedef struct sym_tbl {	/* symbol table */
	Elf_Data *sym_data_pri;	/* primary table */
	Elf_Data *sym_data_aux;	/* auxiliary table */
	size_t	sym_symn_aux;	/* number of entries in auxiliary table */
	size_t	sym_symn;	/* total number of entries in both tables */
	char	*sym_strs;	/* ptr to strings */
	size_t	sym_strsz;	/* size of string table */
	GElf_Shdr sym_hdr_pri;	/* primary symbol table section header */
	GElf_Shdr sym_hdr_aux;	/* auxiliary symbol table section header */
	GElf_Shdr sym_strhdr;	/* string table section header */
	uint_t	*sym_byname;	/* symbols sorted by name */
	uint_t	*sym_byaddr;	/* symbols sorted by addr */
	size_t	sym_count;	/* number of symbols in each sorted list */
} sym_tbl_t;

typedef struct file_info {	/* symbol information for a mapped file */
	plist_t	file_list;	/* linked list */
	char	*file_pname;	/* name from prmap_t */
	struct map_info *file_map;	/* primary (text) mapping */
	int	file_ref;	/* references from map_info_t structures */
	int	file_fd;	/* file descriptor for the mapped file */
	int	file_init;	/* 0: initialization yet to be performed */
	GElf_Half file_etype;	/* ELF e_type from ehdr */
	GElf_Half file_class;	/* ELF e_ident[EI_CLASS] from ehdr */
	rd_loadobj_t *file_lo;	/* load object structure from rtld_db */
	char	*file_lname;	/* load object name from rtld_db */
	char	*file_lbase;	/* pointer to basename of file_lname */
	char	*file_rname;	/* resolved on-disk object pathname */
	char	*file_rbase;	/* pointer to basename of file_rname */
	Elf	*file_elf;	/* ELF handle so we can close */
	sym_tbl_t file_symtab;	/* symbol table */
	sym_tbl_t file_dynsym;	/* dynamic symbol table */
	uintptr_t file_dyn_base;	/* load address for ET_DYN files */
	uintptr_t file_plt_base;	/* base address for PLT */
	size_t	file_plt_size;	/* size of PLT region */
	uintptr_t file_jmp_rel;	/* base address of PLT relocations */
	uintptr_t file_ctf_off;	/* offset of CTF data in object file */
	size_t	file_ctf_size;	/* size of CTF data in object file */
	int	file_ctf_dyn;	/* does the CTF data reference the dynsym */
	void	*file_ctf_buf;	/* CTF data for this file */
	ctf_file_t *file_ctfp;	/* CTF container for this file */
	char	*file_shstrs;	/* section header string table */
	size_t	file_shstrsz;	/* section header string table size */
	uintptr_t *file_saddrs; /* section header addresses */
	uint_t  file_nsaddrs;   /* number of section header addresses */
} file_info_t;

typedef struct map_info {	/* description of an address space mapping */
	prmap_t	map_pmap;	/* /proc description of this mapping */
	file_info_t *map_file;	/* pointer into list of mapped files */
	off64_t map_offset;	/* offset into core file (if core) */
	int map_relocate;	/* associated file_map needs to be relocated */
} map_info_t;

typedef struct elf_file_header { /* extended ELF header */
	unsigned char e_ident[EI_NIDENT];
	Elf64_Half e_type;
	Elf64_Half e_machine;
	Elf64_Word e_version;
	Elf64_Addr e_entry;
	Elf64_Off e_phoff;
	Elf64_Off e_shoff;
	Elf64_Word e_flags;
	Elf64_Half e_ehsize;
	Elf64_Half e_phentsize;
	Elf64_Half e_shentsize;
	Elf64_Word e_phnum;	/* phdr count extended to 32 bits */
	Elf64_Word e_shnum;	/* shdr count extended to 32 bits */
	Elf64_Word e_shstrndx;	/* shdr string index extended to 32 bits */
} elf_file_header_t;

/*
 * Number of buckets in our hash of address->breakpoint.  (We expect very few
 * breakpoints.)
 */

#define BKPT_HASH_BUCKETS	17

/*
 * Handler for breakpoints.
 */

typedef struct bkpt_handler {
	/*
	 * Return nonzero if the process should remain stopped at this
	 * breakpoint, or zero to continue.
	 */
	int (*bkpt_handler) (uintptr_t addr, void *data);
	/*
	 * Clean up any breakpoint state on Prelease() or breakpoint delete.
	 */
	void (*bkpt_cleanup) (void *data);
	void *bkpt_data;
} bkpt_handler_t;

/*
 * An active breakpoint.
 */
typedef struct bkpt {
	struct bkpt *bkpt_next;		/* next in hash chain */
	uintptr_t bkpt_addr;		/* breakpoint address */
	unsigned long orig_insn;	/* original instruction word */
	bkpt_handler_t bkpt_handler;	/* handler for this breakpoint. */
	int after_singlestep;		/* call handler before or after
				           singlestepping? */
} bkpt_t;

/*
 * Handler for exec()s.
 */
typedef void (*exec_handler_fun)(struct ps_prochandle *);

/*
 * A process under management.
 */

typedef struct elf_file {	/* convenience for managing ELF files */
	elf_file_header_t e_hdr; /* Extended ELF header */
	Elf *e_elf;		/* ELF library handle */
	int e_fd;		/* file descriptor */
} elf_file_t;

struct ps_prochandle {
	pid_t	pid;		/* process ID */
	int	state;		/* state of the process, see "libproc.h" */
	int	ptraced;	/* true if ptrace-attached */
	int	ptrace_count;	/* count of Ptrace() calls */
	int	detach;		/* whether to detach when !ptraced and !bkpts */
	int	memfd;		/* /proc/<pid>/mem filedescriptor */
	int	info_valid;	/* if zero, map and file info need updating */
	map_info_t *mappings;	/* cached process mappings */
	size_t	map_count;	/* number of mappings */
	uint_t	num_files;	/* number of file elements in file_info */
	plist_t	file_head;	/* head of mapped files w/ symbol table info */
	auxv_t	*auxv;		/* the process's aux vector */
	int	nauxv;		/* number of aux vector entries */
	bkpt_t	**bkpts;	/* hash of active breakpoints by address */
	uint_t	num_bkpts;	/* number of active breakpoints */
	uintptr_t tracing_bkpt;	/* address of breakpoint we are single-stepping
				   past, if any */
	int	singlestepped;	/* when tracing_bkpt, 1 iff we have done the
				   singlestep. */
	rd_agent_t *rap;	/* rtld_db state */
	map_info_t *map_exec;	/* the mapping for the executable file */
	map_info_t *map_ldso;	/* the mapping for ld.so */
	exec_handler_fun exec_handler;	/* exec() handler */
};

/*
 * Implementation functions in the process control library.
 * These are not exported to clients of the library.
 */
extern	int	Pscantext(struct ps_prochandle *);
extern	void	set_exec_handler(struct ps_prochandle *P,
    exec_handler_fun handler);
extern	void	Pinitsym(struct ps_prochandle *);
extern	map_info_t *Paddr2mptr(struct ps_prochandle *, uintptr_t);
extern	char 	*Pfindexec(struct ps_prochandle *, const char *,
	int (*)(const char *, void *), void *);

extern char	procfs_path[PATH_MAX];

/*
 * Simple convenience.
 */
#define	TRUE	1
#define	FALSE	0

#ifdef	__cplusplus
}
#endif

#endif	/* _PCONTROL_H */
