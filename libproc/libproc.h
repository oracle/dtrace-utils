/*
 * Oracle Linux DTrace.
 * Copyright (c) 2009, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/*
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
#include <setjmp.h>
#include <gelf.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <rtld_db.h>
#include <sys/ctf-api.h>
#include <sys/ptrace.h>
#include <sys/sol_procfs.h>
#include <sys/dtrace_types.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/user.h>

#include <sys/compiler.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Maximum number of digits needed for a pid so we can properly
 * size strings for /proc/$pid/auxv, /proc/$pid/maps, etc.
 * E.g., if procname[] will be "$procfs/$pid/exe", we size
 *     char procname[PATH_MAX + MAXLEN_PID + strlen("/exe") + 1];
 * The null-terminating char of $procfs is overwritten by a '/',
 * and then we add space for the new terminating char at the end.
 */
#define MAXLEN_PID 9

/*
 * Opaque structure tag reference to a process control structure.
 * Clients of libproc cannot look inside the process control structure.
 * The implementation of struct ps_prochandle can change w/o affecting clients.
 */
struct ps_prochandle;
typedef struct ps_prochandle ps_prochandle;

/* State values returned by Pstate() */
#define	PS_RUN		1	/* process is running */
#define	PS_STOP		2	/* process is stopped by SIGSTOP etc */
#define	PS_TRACESTOP	3	/* process is stopped by ptrace() or bkpts */
#define	PS_DEAD		4	/* process is terminated (core file) */

/* Values for Prelease()'s release_mode argument */
#define PS_RELEASE_NORMAL 0	/* ptrace detach, do not kill */
#define PS_RELEASE_KILL 1	/* ptrace detach and kill */
#define PS_RELEASE_NO_DETACH 2	/* no ptrace detach or lock release */

/*
 * Function prototypes for routines in the process control package.
 */
extern	struct ps_prochandle *Pcreate(const char *, char *const *,
    void *, int *);

extern	struct ps_prochandle *Pgrab(pid_t pid, int noninvasiveness,
	int already_ptraced, void *wrap_arg, int *perr);
extern	int	Ptrace(struct ps_prochandle *, int stopped);
extern	void	Ptrace_set_detached(struct ps_prochandle *, boolean_t);

extern	void	Prelease(struct ps_prochandle *, int);
extern	void	Pfree(struct ps_prochandle *P);
extern	void	Puntrace(struct ps_prochandle *, int stay_stopped);
extern	void	Pclose(struct ps_prochandle *);

extern	int	Pmemfd(struct ps_prochandle *);
extern	long	Pwait(struct ps_prochandle *, boolean_t block);
extern	int	Pstate(struct ps_prochandle *);
extern	ssize_t	Pread(struct ps_prochandle *, void *, size_t, uintptr_t);
extern	ssize_t Pread_string(struct ps_prochandle *, char *, size_t, uintptr_t);
extern 	ssize_t	Pread_scalar(struct ps_prochandle *P, void *buf, size_t nbyte,
    size_t nscalar, uintptr_t address);
extern 	ssize_t	Pread_scalar_quietly(struct ps_prochandle *P, void *buf,
    size_t nbyte, size_t nscalar, uintptr_t address, int quietly);
extern	int	Phasfds(struct ps_prochandle *);
extern	void	Pset_procfs_path(const char *);
extern	int	Pdynamically_linked(struct ps_prochandle *);
extern	int	Ptraceable(struct ps_prochandle *);
extern	int	Pelf64(struct ps_prochandle *);

/*
 * Calls that do not take a process structure.  These are used to determine
 * whether it is sensible to grab a pid at all, before the grab takes place.
 */
extern int Pexists(pid_t pid);
extern int Psystem_daemon(pid_t pid, uid_t useruid, const char *sysslice);

/*
 * Read the first argument of the function at which the process P is
 * halted, which must be a pointer.
 *
 * On error, -1 cast to a uintptr_t is returned, and errno is set.
 */
extern	uintptr_t Pread_first_arg(struct ps_prochandle *P);

/*
 * Hook and wrapper functions.  These functions all get their 'arg' argument
 * from the corresponding argument to Pcreate() and Pgrab().
 *
 * Route all calls to ptrace() via the specified wrapper function.
 *
 * _arg_ is constant for all calls using this prochandle until the next call to
 * Pset_ptrace_wrapper().
 */
typedef long ptrace_fun(enum __ptrace_request request, void *arg, pid_t pid,
    void *addr, void *data);

extern	void	Pset_ptrace_wrapper(struct ps_prochandle *P,
    ptrace_fun *wrapper);

/*
 * Likewise, but for Pwait().
 *
 * A program intending to call libproc functions from threads other than those
 * grabbing the process will typically need to wrap both ptrace() and Pwait().
 */
typedef long pwait_fun(struct ps_prochandle *P, void *arg, boolean_t block);

extern	void	Pset_pwait_wrapper(struct ps_prochandle *P, pwait_fun *wrapper);

/*
 * The function that implements the guts of Pwait(), that the Pwait() wrapper
 * function should end up calling (somehow, from some thread or other).  Safe to
 * call only from the thread that did Pgrab() or Pcreate().
 */
extern  long	Pwait_internal(struct ps_prochandle *P, boolean_t block);

/*
 * Register a function to be called around the outermost layer of Ptrace()/
 * Puntrace() calls.  This can, e.g., take a mutex to ensure that other threads
 * do not interfere and unbalance the Ptrace()/Puntrace() stack by issuing
 * further Ptrace()/Puntrace() calls while inside at least one Ptrace().
 *
 * Note that on exit from Pgrab()/Pcreate(), we are in a Ptrace(), so the lock
 * will be taken out then.
 *
 * This locking function is *global* and applies to all libproc calls, but P and
 * the arg are not global.
 */
typedef	void	ptrace_lock_hook_fun(struct ps_prochandle *P, void *arg,
    int ptracing);

extern	void	Pset_ptrace_lock_hook(ptrace_lock_hook_fun *hook);

/*
 * Register a function that returns the address of a per-thread pointer-sized
 * area suitable for storing a jmp_buf, to be called on exec() to register a
 * chain of setjmp loci to unwind out of libproc.  Must never return NULL (but
 * can return a pointer to NULL to indicate that no rethrowing is needed at this
 * point, even if an exec() is detected).
 *
 * As with the ptrace lock hook, this is global, for the same reason (it must
 * work during Pcreate() and Pgrab()).
 */
typedef	jmp_buf **libproc_unwinder_pad_fun(struct ps_prochandle *P);
extern	void	Pset_libproc_unwinder_pad(libproc_unwinder_pad_fun *unwinder_pad);

/*
 * Breakpoints.
 *
 * This is not a completely safe interface: it cannot handle breakpoints on
 * signal return.
 */
extern	int	Pbkpt(struct ps_prochandle *P, uintptr_t addr, int after_singlestep,
    int (*bkpt_handler)(uintptr_t addr, void *data),
    void (*bkpt_cleanup)(void *data),
    void *data);
extern	int	Pbkpt_notifier(struct ps_prochandle *P, uintptr_t addr, int after_singlestep,
    void (*bkpt_handler)(uintptr_t addr, void *data),
    void (*bkpt_cleanup)(void *data),
    void *data);
extern	void	Punbkpt(struct ps_prochandle *P, uintptr_t address);
extern	int	Pbkpt_continue(struct ps_prochandle *P);
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
extern int Plookup_by_addr(struct ps_prochandle *, uintptr_t, const char **,
			   GElf_Sym *);

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
extern const prmap_file_t *Pinode_to_file_map(struct ps_prochandle *,
    dev_t, ino_t);
extern char *Pmap_mapfile_name(struct ps_prochandle *P, const prmap_t *mapp);

extern char *Pobjname(struct ps_prochandle *, uintptr_t, char *, size_t);
extern int Plmid(struct ps_prochandle *, uintptr_t, Lmid_t *);
extern uint64_t Pgetauxval(struct ps_prochandle *P, int type);

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
