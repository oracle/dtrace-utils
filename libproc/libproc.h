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
 * Copyright 2009, 2012, 2013 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 *
 * Portions Copyright 2007 Chad Mynhier
 */

/*
 * Interfaces available from the process control library, libproc.
 */

#ifndef	_LIBPROC_H
#define	_LIBPROC_H

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <nlist.h>
#include <gelf.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <rtld_db.h>
#include <sys/ctf_api.h>
#include <sys/procfs.h>
#include <sys/dtrace_types.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <sys/time.h>
#include <sys/user.h>

#include <sys/compiler.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Opaque structure tag reference to a process control structure.
 * Clients of libproc cannot look inside the process control structure.
 * The implementation of struct ps_prochandle can change w/o affecting clients.
 */
struct ps_prochandle;
typedef struct ps_prochandle ps_prochandle;

extern	int	_libproc_debug;	/* set non-zero to enable debugging fprintfs */

#if defined(__sparc)
#define	R_RVAL1	R_O0		/* register holding a function return value */
#define	R_RVAL2	R_O1		/* 32 more bits for a 64-bit return value */
#endif	/* __sparc */

#if defined(__amd64)
#define	R_PC	REG_RIP
#define	R_SP	REG_RSP
#define	R_RVAL1	REG_RAX		/* register holding a function return value */
#define	R_RVAL2	REG_RDX		/* 32 more bits for a 64-bit return value */
#elif defined(__i386)
#define	R_PC	EIP
#define	R_SP	UESP
#define	R_RVAL1	EAX		/* register holding a function return value */
#define	R_RVAL2	EDX		/* 32 more bits for a 64-bit return value */
#endif	/* __amd64 || __i386 */

#define	R_RVAL	R_RVAL1		/* simple function return value register */

/* State values returned by Pstate() */
#define	PS_RUN		1	/* process is running */
#define	PS_STOP		2	/* process is stopped */
#define	PS_TRACESTOP	3	/* process is stopped by ptrace() */
#define	PS_DEAD		4	/* process is terminated (core file) */

/* values for type */
#define	AT_BYVAL	1
#define	AT_BYREF	2

/* values for inout */
#define	AI_INPUT	1
#define	AI_OUTPUT	2
#define	AI_INOUT	3

/* maximum number of syscall arguments */
#define	MAXARGS		8

/* maximum size in bytes of a BYREF argument */
#define	MAXARGL		(4*1024)

/*
 * Function prototypes for routines in the process control package.
 */
extern	struct ps_prochandle *Pcreate(const char *, char *const *,
    int *, size_t);

extern	struct ps_prochandle *Pgrab(pid_t, int *);
extern	int	Ptrace(struct ps_prochandle *, int stopped);
extern	void	Ptrace_set_detached(struct ps_prochandle *, boolean_t);

extern	void	Prelease(struct ps_prochandle *, boolean_t);
extern	void	Puntrace(struct ps_prochandle *, int state);
extern	void	Pclose(struct ps_prochandle *);

extern	int	Pmemfd(struct ps_prochandle *);
extern	int	Pwait(struct ps_prochandle *, boolean_t block);
extern	int	Pstate(struct ps_prochandle *);
extern	ssize_t	Pread(struct ps_prochandle *, void *, size_t, uintptr_t);
extern	ssize_t Pread_string(struct ps_prochandle *, char *, size_t, uintptr_t);
extern 	ssize_t	Pread_scalar(struct ps_prochandle *P, void *buf, size_t nbyte,
    size_t nscalar, uintptr_t address);
extern	int	Phasfds(struct ps_prochandle *);
extern	void	Pset_procfs_path(const char *);

/*
 * Breakpoints.
 *
 * This is not a completely safe interface: it cannot handle breakpoints on
 * signal return.
 */
extern	int	Pbkpt(struct ps_prochandle *P, uintptr_t addr, int after_singlestep,
    int (*bkpt_handler) (uintptr_t addr, void *data),
    void (*bkpt_cleanup) (void *data),
    void *data);
extern	void	Punbkpt(struct ps_prochandle *P, uintptr_t address);
extern	void	Pbkpt_continue(struct ps_prochandle *P);
extern 	uintptr_t Pbkpt_addr(struct ps_prochandle *P);

/*
 * Symbol table interfaces.
 */

/*
 * Pseudo-names passed to Plookup_by_name() for well-known load objects.
 */
#define	PR_OBJ_EXEC	((const char *)0)	/* search the executable file */
#define	PR_OBJ_LDSO	((const char *)1)	/* search ld.so */
#define	PR_OBJ_EVERY	((const char *)-1)	/* search every load object */

/*
 * Special Lmid_t passed to Plookup_by_lmid() to search all link maps.  The
 * special values LM_ID_BASE and LM_ID_LDSO from <link.h> may also be used.
 * If PR_OBJ_EXEC is used as the object name, the lmid must be PR_LMID_EVERY
 * or LM_ID_BASE in order to return a match.  If PR_OBJ_LDSO is used as the
 * object name, the lmid must be PR_LMID_EVERY or LM_ID_LDSO to return a match.
 */
#define	PR_LMID_EVERY	((Lmid_t)-1UL)		/* search every link map */

/*
 * 'object_name' is the name of a load object obtained from an
 * an iteration over the process's mapped objects (Pobject_iter),
 * or else it is one of the special PR_OBJ_* values above.
 */
extern int Plookup_by_addr(struct ps_prochandle *,
    uintptr_t, char *, size_t, GElf_Sym *);

typedef struct prsyminfo {
	const char	*prs_object;		/* object name */
	const char	*prs_name;		/* symbol name */
	Lmid_t		prs_lmid;		/* link map id */
	uint_t		prs_id;			/* symbol id */
	uint_t		prs_table;		/* symbol table id */
} prsyminfo_t;

extern int Pxlookup_by_name(struct ps_prochandle *,
    Lmid_t, const char *, const char *, GElf_Sym *, prsyminfo_t *);

typedef int proc_map_f(void *, const prmap_t *, const char *);

extern int Pobject_iter(struct ps_prochandle *, proc_map_f *, void *);

extern const prmap_t *Paddr_to_map(struct ps_prochandle *, uintptr_t);
extern const prmap_t *Pname_to_map(struct ps_prochandle *, const char *);
extern const prmap_t *Plmid_to_map(struct ps_prochandle *,
    Lmid_t, const char *);

extern char *Pobjname(struct ps_prochandle *, uintptr_t, char *, size_t);
extern int Plmid(struct ps_prochandle *, uintptr_t, Lmid_t *);

/*
 * Symbol table iteration interface.
 */
typedef int proc_sym_f(void *, const GElf_Sym *, const char *);

extern int Psymbol_iter_by_addr(struct ps_prochandle *,
    const char *, int, int, proc_sym_f *, void *);

/*
 * 'which' selects which symbol table and can be one of the following.
 */
#define	PR_SYMTAB	1
#define	PR_DYNSYM	2
/*
 * 'type' selects the symbols of interest by binding and type.  It is a bit-
 * mask of one or more of the following flags, whose order MUST match the
 * order of STB and STT constants in <sys/elf.h>.
 */
#define	BIND_LOCAL	0x0001
#define	BIND_GLOBAL	0x0002
#define	BIND_WEAK	0x0004
#define	BIND_ANY (BIND_LOCAL|BIND_GLOBAL|BIND_WEAK)
#define	TYPE_NOTYPE	0x0100
#define	TYPE_OBJECT	0x0200
#define	TYPE_FUNC	0x0400
#define	TYPE_SECTION	0x0800
#define	TYPE_FILE	0x1000
#define	TYPE_ANY (TYPE_NOTYPE|TYPE_OBJECT|TYPE_FUNC|TYPE_SECTION|TYPE_FILE)

/*
 * This should be called when an RD_DLACTIVITY event with the
 * RD_CONSISTENT state occurs via librtld_db's event mechanism.
 * This makes libproc's address space mappings and symbol tables current.
 * The variant Pupdate_syms() can be used to preload all symbol tables as well.
 */
extern void Pupdate_maps(struct ps_prochandle *);
extern void Pupdate_syms(struct ps_prochandle *);

/*
 * This must be called after the victim process performs a successful
 * exec() if any of the symbol table interface functions have been called
 * prior to that point.  This is essential because an exec() invalidates
 * all previous symbol table and address space mapping information.
 * It is always safe to call, but if it is called other than after an
 * exec() by the victim process it just causes unnecessary overhead.
 *
 * The rtld_db agent handle obtained from a previous call to Prd_agent() is
 * made invalid by Preset_maps() and Prd_agent() must be called again to get
 * the new handle.
 */
extern void Preset_maps(struct ps_prochandle *);

/*
 * Return 1 if this address is within a valid mapping, file-backed mapping, or
 * writable mapping, respectively.
 */
extern int Pvalid_mapping(struct ps_prochandle *P, uintptr_t addr);
extern int Pfile_mapping(struct ps_prochandle *P, uintptr_t addr);
extern int Pwritable_mapping(struct ps_prochandle *P, uintptr_t addr);

extern pid_t Pgetpid(struct ps_prochandle *);

/*
 * Return the librtld_db agent handle for the victim process.
 * The handle will become invalid at the next successful exec() and the
 * client (caller of proc_rd_agent()) must not use it beyond that point.
 * If the process is already dead, there's nothing we can do.
 */
rd_agent_t *Prd_agent(struct ps_prochandle *);

#ifdef	__cplusplus
}
#endif

#endif	/* _LIBPROC_H */
