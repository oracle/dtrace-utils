/*
 * Oracle Linux DTrace.
 * Copyright (c) 2008, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
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
 * The unwinder pad function.  Used by code throughout libproc, so extern.
 */
extern libproc_unwinder_pad_fun *libproc_unwinder_pad;

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
	int (*bkpt_handler)(uintptr_t addr, void *data);
	/*
	 * Clean up any breakpoint state on Prelease() or breakpoint delete.
	 */
	void (*bkpt_cleanup)(void *data);
	void *bkpt_data;
} bkpt_handler_t;

/*
 * An active breakpoint.
 */
typedef struct bkpt {
	struct bkpt *bkpt_next;		/* next in hash chain */
	struct bkpt *bkpt_singlestep;	/* if non-NULL, this is a temporary
					   breakpoint used for singlestepping
					   past this other breakpoint. */
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
	int released;			/* 1 if released */
	size_t l_searchlist_offset;	/* Offset of the l_searchlist in the
					   link map structure. */
	int r_version;			/* the version of the r_debug interface */
	uintptr_t r_brk_addr;		/* if nonzero, the address of r_brk */
	uintptr_t rtld_global_addr;	/* if nonzero, the address of
					   _rtld_global */
	size_t	dl_nns_offset;		/* Offset of the dl_nns from rtld_global.  */
	size_t	dl_load_lock_offset;	/* Offset of the dl_load_lock from
					 * rtld_global.  */
	size_t	g_debug_offset;		/* Offset of the g_debug element from
					 * its expected value, G_DEBUG.  */
	size_t	link_namespaces_size;	/* Apparent size of "struct link
					   namespaces" in this glibc. */
	int	rd_monitoring;		/* 1 whenever rtld_db has a breakpoint
					   set on the dynamic linker. */
	int	rd_monitor_suppressed;	/* 1 if rd monitoring is off forever */
	rd_event_fun rd_event_fun;	/* function to call on rtld event */
	void	*rd_event_data;		/* state passed to rtld_event_fun */

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

	int	lmid_halted;		/* if nonzero, the process was halted by
					   rd_ldso_nonzero_lmid_consistent_begin(). */
	int	lmid_bkpted;		/* halted on bkpt by rlnlcb(). */
	int	lmid_incompatible_glibc; /* glibc data structure change. */
};

/*
 * Previous states of the process.
 */
typedef struct prev_states {
	dt_list_t states_list;	/* linked list (stack, pushpopped at head) */
	int state;		/* previous state */
} prev_states_t;

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
	dt_list_t ptrace_states; /* states of higher Ptrace() levels */
	int	ptrace_halted;	/* true if halted by Ptrace() call */
        int	pending_stops;	/* number of SIGSTOPs Ptrace() has sent that
				   have yet to be consumed */
	int	awaiting_pending_stops; /* if 1, a pending stop is being waited
					   for: all blocking Pwait()s when
					   pending_stops == 0 are converted
					   to nonblocking */
	int	group_stopped;	/* if 1, in group-stop */
	int	listening;	/* if 1, in PTRACE_LISTEN */
	int	detach;		/* whether to detach when !ptraced and !bkpts */
	int	no_dyn;		/* true if this is probably statically linked */
	int	memfd;		/* /proc/<pid>/mem filedescriptor */
	int	mapfilefd;	/* /proc/<pid>/map_files directory fd */
	int	info_valid;	/* if zero, map and file info need updating */
	int	lmids_valid;	/* 0 if we haven't yet scanned the link map */
	int	elf64;		/* if nonzero, this is a 64-bit process */
	int	elf_machine;	/* the e_machine of this process */
	map_info_t *mappings;	/* process mappings, sorted by address */
	size_t	num_mappings;	/* number of mappings */
	prmap_file_t **map_files; /* hash of mappings by filename */
	prmap_file_t **map_inum; /* hash of mappings by (dev, inode) */
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
	ptrace_fun *ptrace_wrap; /* ptrace() wrapper */
	pwait_fun *pwait_wrap;	 /* pwait() wrapper */
	void *wrap_arg;		 /* args for hooks and wrappers */
};

/*
 * Implementation functions in the process control library.
 * These are not exported to clients of the library.
 */
extern	int	Psym_init(struct ps_prochandle *);
extern	void	Psym_free(struct ps_prochandle *);
extern	int	Pread_isa_info(struct ps_prochandle *P, const char *procname);
extern	void	Preadauxvec(struct ps_prochandle *P);
extern  uintptr_t Pget_bkpt_ip(struct ps_prochandle *P, int expect_esrch);
extern  long	Preset_bkpt_ip(struct ps_prochandle *P, uintptr_t addr);
extern	char *	Pget_proc_status(pid_t pid, const char *field);
extern	int	Pmapfilefd(struct ps_prochandle *P);

#ifdef NEED_SOFTWARE_SINGLESTEP
extern	uintptr_t	Pget_next_ip(struct ps_prochandle *P);
#endif

extern	uintptr_t r_debug(struct ps_prochandle *P);
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
