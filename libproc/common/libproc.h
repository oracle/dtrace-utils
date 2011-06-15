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
 * Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 *
 * Portions Copyright 2007 Chad Mynhier
 */

/*
 * Interfaces available from the process control library, libproc.
 *
 * libproc provides process control functions for the /proc tools
 * (commands in /usr/proc/bin), /usr/bin/truss, and /usr/bin/gcore.
 * libproc is a private support library for these commands only.
 * It is _not_ a public interface, although it might become one
 * in the fullness of time, when the interfaces settle down.
 *
 * In the meantime, be aware that any program linked with libproc in this
 * release of Solaris is almost guaranteed to break in the next release.
 *
 * In short, do not use this header file or libproc for any purpose.
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
#include <libctf.h>
#include <sys/procfs.h>
#include <sys/auxv.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <sys/time.h>

#include <sys/compiler.h>
#include <procfs_service.h>


#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Opaque structure tag reference to a process control structure.
 * Clients of libproc cannot look inside the process control structure.
 * The implementation of struct ps_prochandle can change w/o affecting clients.
 */
struct ps_prochandle;

/*
 * Opaque structure tag reference to an lwp control structure.
 */
struct ps_lwphandle;

extern	int	_libproc_debug;	/* set non-zero to enable debugging fprintfs */
extern	int	_libproc_no_qsort;	/* set non-zero to inhibit sorting */
					/* of symbol tables */
extern	int	_libproc_incore_elf;	/* only use in-core elf data */

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

/* maximum sizes of things */
#define	PRMAXSIG	(32 * sizeof (sigset_t) / sizeof (uint32_t))
#define	PRMAXFAULT	(32 * sizeof (fltset_t) / sizeof (uint32_t))
#define	PRMAXSYS	(32 * sizeof (sysset_t) / sizeof (uint32_t))

/* State values returned by Pstate() */
#define	PS_RUN		1	/* process is running */
#define	PS_STOP		2	/* process is stopped */
#define	PS_TRACESTOP	3	/* process is stopped by ptrace() */
#define	PS_DEAD		4	/* process is terminated (core file) */

/* Error codes from Pcreate() */
#define	C_STRANGE	-1	/* Unanticipated error, errno is meaningful */
#define	C_FORK		1	/* Unable to fork */
#define	C_PERM		2	/* No permission (file set-id or unreadable) */
#define	C_NOEXEC	3	/* Cannot execute file */
#define	C_INTR		4	/* Interrupt received while creating */
#define	C_LP64		5	/* Program is _LP64, self is _ILP32 */
#define	C_NOENT		6	/* Cannot find executable file */
#define C_FDS           7	/* Out of file descriptors */

/* Error codes from Pgrab() */
#define	G_STRANGE	-1	/* Unanticipated error, errno is meaningful */
#define	G_NOPROC	1	/* No such process */
#define	G_NOCORE	2	/* No such core file */
#define	G_NOPROCORCORE	3	/* No such proc or core (for proc_arg_grab) */
#define	G_NOEXEC	4	/* Cannot locate executable file */
#define	G_ZOMB		5	/* Zombie process */
#define	G_PERM		6	/* No permission */
#define	G_BUSY		7	/* Another process has control */
#define	G_SYS		8	/* System process */
#define	G_SELF		9	/* Process is self */
#define	G_INTR		10	/* Interrupt received while grabbing */
#define	G_LP64		11	/* Process is _LP64, self is ILP32 */
#define	G_FORMAT	12	/* File is not an ELF format core file */
#define	G_ELF		13	/* Libelf error, elf_errno() is meaningful */
#define	G_NOTE		14	/* Required PT_NOTE Phdr not present in core */
#define	G_ISAINVAL	15	/* Wrong ELF machine type */
#define	G_BADLWPS	16	/* Bad '/lwps' specification */
#define	G_NOFD		17	/* No more file descriptors */


typedef	struct {	/* argument descriptor for system call (Psyscall) */
	long	arg_value;	/* value of argument given to system call */
	void	*arg_object;	/* pointer to object in controlling process */
	char	arg_type;	/* AT_BYVAL, AT_BYREF */
	char	arg_inout;	/* AI_INPUT, AI_OUTPUT, AI_INOUT */
	ushort_t arg_size;	/* if AT_BYREF, size of object in bytes */
} argdes_t;

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
extern struct ps_prochandle *Pcreate(const char *, char *const *,
    int *, char *, size_t);

extern const char *Pcreate_error(int);

extern struct ps_prochandle *Pgrab(pid_t, int *);
extern const char *Pgrab_error(int);

extern	void	Prelease(struct ps_prochandle *, boolean_t);
extern	void	Pfree(struct ps_prochandle *);
extern	void	Pclose(struct ps_prochandle *);

extern	int	Pmemfd(struct ps_prochandle *);
extern	char   *Pbrandname(struct ps_prochandle *, char *, size_t);
extern	int	Pcreate_agent(struct ps_prochandle *);
extern	void	Pdestroy_agent(struct ps_prochandle *);
extern	int	Pwait(struct ps_prochandle *, uint_t);
extern	int	Pstop(struct ps_prochandle *, uint_t);
extern	int	Pdstop(struct ps_prochandle *);
extern	int	Pstate(struct ps_prochandle *);
extern	int	Pgetareg(struct ps_prochandle *, int, prgreg_t *);
extern	int	Pputareg(struct ps_prochandle *, int, prgreg_t);
extern	int	Psetrun(struct ps_prochandle *);
extern	ssize_t	Pread(struct ps_prochandle *, void *, size_t, uintptr_t);
extern	ssize_t Pread_string(struct ps_prochandle *, char *, size_t, uintptr_t);
extern	ssize_t	Pwrite(struct ps_prochandle *, const void *, size_t, uintptr_t);
extern	int	Pdelbkpt(struct ps_prochandle *, uintptr_t, ulong_t);
extern	void	Psetsignal(struct ps_prochandle *, const sigset_t *);
extern	int	Phasfds(struct ps_prochandle *);

extern	int	Psyscall(struct ps_prochandle *, sysret_t *,
			int, uint_t, argdes_t *);
extern	int	Pisprocdir(struct ps_prochandle *, const char *);

/*
 * Symbol table interfaces.
 */

/*
 * Pseudo-names passed to Plookup_by_name() for well-known load objects.
 * NOTE: It is required that PR_OBJ_EXEC and PR_OBJ_LDSO exactly match
 * the definitions of PS_OBJ_EXEC and PS_OBJ_LDSO from <proc_service.h>.
 */
#define	PR_OBJ_EXEC	((const char *)0)	/* search the executable file */
#define	PR_OBJ_LDSO	((const char *)1)	/* search ld.so.1 */
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

typedef int proc_env_f(void *, struct ps_prochandle *, const char *);

extern void Pset_procfs_path(const char *);

/*
 * Symbol table iteration interface.
 */
typedef int proc_sym_f(void *, const GElf_Sym *, const char *);
typedef int proc_xsym_f(void *, const GElf_Sym *, const char *,
    const prsyminfo_t *);

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
 */
extern void Pupdate_maps(struct ps_prochandle *);

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
 * Given an address, Ppltdest() determines if this is part of a PLT, and if
 * so returns a pointer to the symbol name that will be used for resolution.
 * If the specified address is not part of a PLT, the function returns NULL.
 */
extern const char *Ppltdest(struct ps_prochandle *, uintptr_t);

/*
 * The following functions define a set of passive interfaces: libproc provides
 * default, empty definitions that are called internally.  If a client wishes
 * to override these definitions, it can simply provide its own version with
 * the same signature that interposes on the libproc definition.
 *
 * If the client program wishes to report additional error information, it
 * can provide its own version of Perror_printf.
 *
 */
_dt_printflike_(2,3)
extern void Perror_printf(struct ps_prochandle *P _dt_unused_,
			  const char *format _dt_unused_, ...);

extern int Pgcore(struct ps_prochandle *, const char *, core_content_t);
extern int Pfgcore(struct ps_prochandle *, int, core_content_t);
extern core_content_t Pcontent(struct ps_prochandle *);

extern pid_t ps_getpid(struct ps_prochandle *);

#ifdef	__cplusplus
}
#endif

#endif	/* _LIBPROC_H */
