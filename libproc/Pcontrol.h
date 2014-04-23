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
 * Copyright 2008, 2011 -- 2014 Oracle, Inc.  All rights reserved.
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
#include <limits.h>
#include <dtrace.h>
#include <dt_list.h>
#include <setjmp.h>
#include <sys/ptrace.h>

#include <sys/auxv.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Definitions of the process control structures, internal to libproc.
 * These may change without affecting clients of libproc.
 */

/*
 * sym_tbl_t contains a primary and an (optional) auxiliary symbol table, which
 * we wish to treat as a single logical symbol table.  In this logical table,
 * the data from the auxiliary table precedes that from the primary.  Symbol
 * indices start at [0], which is the first item in the auxiliary table
 * if there is one.  The sole purpose for this is so that we can treat the
 * combination of .ldynsym and .dynsym sections as a logically single
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

/*
 * This structure persists even across shared library loads and unloads: it is
 * reference-counted by file_ref and deallocated only when this reaches zero.
 *
 * However, file_lo and file_symsearch have different lifetimes: as with the
 * map_info, the latter is discarded whenever !ps_prochandle_t.info_valid, and
 * the former is malloced only once but is overwritten whenever a map_iter() is
 * carried out (generally right after info_valid becomes 1 again).  Within the
 * file_lo, rl_scope is also dynamically allocated, but is freed and reallocated
 * whenever map_iter() is run.
 */
struct map_info;
typedef struct file_info {	/* symbol information for a mapped file */
	dt_list_t file_list;	/* linked list */
	ssize_t	file_map; 	/* primary (text) mapping idx, or -1 if none */
	char	*file_pname;	/* name from prmap_file_t */
	dev_t	file_dev;	/* device number of file */
	ino_t	file_inum;	/* inode number of file */
	int	file_ref;	/* references from map_info_t structures */
	int	file_fd;	/* file descriptor for the mapped file */
	int	file_init;	/* 0: initialization yet to be performed */
	GElf_Half file_etype;	/* ELF e_type from ehdr */
	rd_loadobj_t *file_lo;	/* load object structure from rtld_db */
	char	*file_lname;	/* load object name from rtld_db */
	char	*file_lbase;	/* pointer to basename of file_lname */
	Elf	*file_elf;	/* ELF handle */
	struct file_info **file_symsearch; /* Symbol search path */
	unsigned int file_nsymsearch; /* number of items therein */
	sym_tbl_t file_symtab;	/* symbol table */
	sym_tbl_t file_dynsym;	/* dynamic symbol table */
	uintptr_t file_dyn_base;	/* load address for ET_DYN files */
	char	*file_shstrs;	/* section header string table */
	size_t	file_shstrsz;	/* section header string table size */
} file_info_t;

/*
 * The mappings are stored in ps_prochandle_t.mappings; the hash of mapping
 * filenames are stored in ps_prochandle_t.map_files.
 *
 * There is no ownership relationship between the prmap_file_t and the prmap_t:
 * they just point to each other.
 */
typedef struct map_info {	/* description of an address space mapping */
	prmap_t	*map_pmap;	/* /proc description of this mapping */
	file_info_t *map_file;	/* pointer into list of mapped files */
} map_info_t;

/*
 * Number of buckets in our hashes of process -> mapping name and
 * address->breakpoint.  (We expect quite a lot of mappings, and
 * very few breakpoints.)
 */

#define MAP_HASH_BUCKETS	277
#define BKPT_HASH_BUCKETS	17

/*
 * Handler for breakpoints.  (Notifiers use the same data structure, but cast
 * the handler to return void.)
 */

typedef struct bkpt_handler {
	dt_list_t notifier_list;	/* linked list (notifiers only) */
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
	dt_list_t bkpt_notifiers;	/* notifier chain */
	int after_singlestep;		/* call handler before or after
				           singlestepping? */
	int in_handler;			/* in the handler now */
	int pending_removal;		/* some handler has called Punbkpt() */
} bkpt_t;

/*
 * librtld_db agent state.
 */

struct rd_agent {
	struct ps_prochandle *P;	/* pointer back to our ps_prochandle */
	int maps_ready;			/* 1 if the link maps are ready */
	size_t l_searchlist_offset;	/* Offset of the l_searchlist in the
					   link map structure. */
	uintptr_t r_brk_addr;		/* if nonzero, the address of r_brk */
	uintptr_t rtld_global_addr;	/* if nonzero, the address of
					   _rtld_global */
	int	rd_monitoring;		/* 1 whenever rtld_db has a breakpoint
					   set on the dynamic linker. */
	rd_event_fun rd_event_fun;	/* function to call on rtld event */
	void	*rd_event_data;		/* state passed to rtld_event_fun */

	/*
	 * This is used to detect if an exec() has happened deep inside the call
	 * stack of rtld_db.
	 */
	int	exec_detected;
	jmp_buf	*exec_jmp;

	/*
	 * Transition to an inconsistent state is barred.  (If multiple such
	 * prohibitions are in force, this is >1).  If we are actively waiting
	 * for a transition, ic_transitioned > 0 indicates that we have hit one:
	 * otherwise, it is not meaningful.  If stop_on_consistent is 1, stop on
	 * transition to a consistent state, not an inconsistent one.
	 */

	int	no_inconsistent;
	int	stop_on_consistent;
	int	ic_transitioned;
	int	prev_state;		/* process state before consistency
					   checking was enabled */

	int	lmid_halted;		/* if nonzero, the process was halted by
					   rd_ldso_nonzero_lmid_consistent_begin(). */
	int	lmid_halt_prev_state;	/* process state before said halt */
	int	lmid_bkpted;		/* halted on bkpt by rlnlcb(). */
	int	lmid_incompatible_glibc; /* glibc data structure change. */
};

/*
 * Handler for exec()s.
 */
typedef void exec_handler_fun(struct ps_prochandle *);

/*
 * A process under management.
 */

struct ps_prochandle {
	pid_t	pid;		/* process ID */
	int	state;		/* state of the process, see "libproc.h" */
	int	released;	/* true if released but not yet freed */
	int	ptraced;	/* true if ptrace-attached */
	int	noninvasive;	/* true if this is a noninvasive grab */
	int	ptrace_count;	/* count of Ptrace() calls */
	int	ptrace_halted;	/* true if halted by Ptrace() call */
	int	pending_stops;	/* number of SIGSTOPs Ptrace() has sent that
				   have yet to be consumed */
	int	pending_pre_exec; /* number of pending_stops that were sent
				     before a detected exec() */
	int	awaiting_pending_stops; /* if 1, a pending stop is being waited
					   for: all blocking Pwait()s when
					   pending_stops == 0 are converted
					   to nonblocking */
	int	detach;		/* whether to detach when !ptraced and !bkpts */
	int	no_dyn;		/* true if this is probably statically linked */
	int	memfd;		/* /proc/<pid>/mem filedescriptor */
	int	info_valid;	/* if zero, map and file info need updating */
	int	lmids_valid;	/* 0 if we haven't yet scanned the link map */
	int	elf64;		/* if nonzero, this is a 64-bit process */
	int	elf_machine;	/* the e_machine of this process */
	map_info_t *mappings;	/* process mappings, sorted by address */
	size_t	num_mappings;	/* number of mappings */
	prmap_file_t **map_files; /* hash of mappings by filename */
	uint_t  num_files;	/* number of file elements in file_list */
	dt_list_t file_list;	/* list of mapped files w/ symbol table info */
	auxv_t	*auxv;		/* the process's aux vector */
	int	nauxv;		/* number of aux vector entries */
	bkpt_t	**bkpts;	/* hash of active breakpoints by address */
	uint_t	num_bkpts;	/* number of active breakpoints */
	uintptr_t tracing_bkpt;	/* address of breakpoint we are single-stepping
				   past, if any */
	int	bkpt_halted;	/* halted at breakpoint by handler */
	int	bkpt_consume;	/* Ask Pwait() to consume any breakpoint traps */
	uintptr_t r_debug_addr;	/* address of r_debug in the child */
	rd_agent_t *rap;	/* rtld_db state */
	ssize_t map_exec;	/* the index of the executable mapping */
	ssize_t map_ldso;	/* the index of the ld.so mapping */
	exec_handler_fun *exec_handler;	/* exec() handler */
	ptrace_fun *ptrace_wrap; /* ptrace() wrapper */
	pwait_fun *pwait_wrap;	 /* pwait() wrapper */
	void *wrap_arg;		 /* args for hooks and wrappers */
};

/*
 * Implementation functions in the process control library.
 * These are not exported to clients of the library.
 */
extern	void	Psym_init(struct ps_prochandle *);
extern	void	Psym_free(struct ps_prochandle *);
extern	void	Psym_release(struct ps_prochandle *);
extern	int	Pread_isa_info(struct ps_prochandle *P, const char *procname);
extern	void	Preadauxvec(struct ps_prochandle *P);
extern	uintptr_t r_debug(struct ps_prochandle *P);
extern	void	set_exec_handler(struct ps_prochandle *P,
    exec_handler_fun *handler);
extern	char	procfs_path[PATH_MAX];

/*
 * The wrapper functions are somewhat inconsistently named, because we can
 * rename the guts of Pwait() to Pwait_internal() while retaining the helpful
 * external name but have no such luxury with ptrace().  hooked_ptrace() is for
 * our internal use, to dispatch on to the wrapper.
 */
extern	long wrapped_ptrace(struct ps_prochandle *P, enum __ptrace_request request,
    pid_t pid, ...);

/*
 * Poison all calls to ptrace() from everywhere but the ptrace hook.
 */
#ifndef DO_NOT_POISON_PTRACE
#pragma GCC poison ptrace
#endif

/*
 * Routine to print debug messages.
 */
_dt_printflike_(1,2)
extern void _dprintf(const char *, ...);

/*
 * Simple convenience.
 */
#define	TRUE	1
#define	FALSE	0

#ifdef	__cplusplus
}
#endif

#endif	/* _PCONTROL_H */
