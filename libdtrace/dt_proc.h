/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_PROC_H
#define	_DT_PROC_H

#include <rtld_db.h>
#include <libproc.h>
#include <dtrace.h>
#include <pthread.h>
#include <dt_list.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct dt_proc {
	dt_list_t dpr_list;		/* prev/next pointers for lru chain */
	struct dt_proc *dpr_hash;	/* next pointer for pid hash chain */
	dtrace_hdl_t *dpr_hdl;		/* back pointer to libdtrace handle */
	struct ps_prochandle *dpr_proc;	/* proc handle for libproc calls */
	char dpr_errmsg[BUFSIZ];	/* error message */
	pthread_mutex_t dpr_lock;	/* lock around many libproc operations,
					   and our condition variables */
	pthread_t dpr_lock_holder;	/* holder of the dpr_lock, if nonzero
					   lock count */
	unsigned long dpr_lock_count_main; /* main-thread dpr_lock count */
	unsigned long dpr_lock_count_ctrl; /* ctrl-thread dpr_lock count */
	pthread_cond_t dpr_cv;		/* cond for startup/stop/quit/done */
	pthread_cond_t dpr_msg_cv;	/* cond for msgs from main thread */
	pthread_t dpr_tid;		/* control thread (or zero if none) */
	pid_t dpr_pid;			/* pid of process */
	int dpr_fd;			/* waitfd for process */
	int dpr_proxy_fd[2];		/* proxy request pipe from main thread */
	uint_t dpr_refs;		/* reference count */
	uint8_t dpr_stop;		/* stop mask: see flag bits below */
	uint8_t dpr_done;		/* done flag: ctl thread has exited */
	uint8_t dpr_created;            /* proc flag: true if we created this
					   process, false if we grabbed it */
	uint8_t dpr_monitoring;		/* true if we should background-monitor
					   the process right now */
	uint8_t dpr_ending;		/* true in the middle of process death,
					   when unlocking should null out the
					   dpr_tid */
	uint8_t dpr_notifiable;		/* true if we should call notifiers
					   when this process dies */
	uint8_t dpr_awaiting_dlactivity; /* true if a dlopen()/dlclose() has
					    been seen and the victim ld.so is
					    not yet in a consistent state */

	/*
	 * Proxying. These structures encode the return type and parameters of
	 * all libproc functions which are unsafe to call from multiple threads
	 * for which we provide proxies.  The dpr_libproc_rq simply holds the
	 * address of the currently-executing proxy function (which is private
	 * to libproc.c, so not interpretable from here).  Executing one
	 * proxy from inside another is not possible, because each proxy
	 * degenerates to an immediate call if it is executed from the
	 * process-control thread itself.
	 *
	 * Currently proxied requests are proxy_pwait(), proxy_ptrace() and
	 * dt_proc_continue(): the latter takes no arguments.  In all these
	 * cases, if an exec is detected, dpr_proxy_exec_retry is set on return:
	 * it is then the proxy's responsibility to rethrow and unwind all the
	 * way out, destroy and reattach to the libproc structure, and retry
	 * whatever it was doing.
	 *
	 * In addition, communication between the main thread and
	 * process-control thread also uses this mechanism but does not
	 * participate in the exec-detection mechanism.  This uses requests
	 * proxy_reattach(), proxy_monitor(), and proxy_quit().
	 */
	long (*dpr_proxy_rq)();
	long dpr_proxy_ret;
	long dpr_proxy_exec_retry;
	int dpr_proxy_errno;
	union {
		struct {
			enum __ptrace_request request;
			pid_t pid;
			void *addr;
			void *data;
		} dpr_ptrace;

		struct {
			struct ps_prochandle *P;
			boolean_t block;
		} dpr_pwait;

		struct {
			boolean_t monitor;
		} dpr_monitor;

		struct {
			int err;
		} dpr_quit;
	} dpr_proxy_args;
} dt_proc_t;

typedef struct dt_proc_notify {
	dt_proc_t *dprn_dpr;		/* process associated with the event */
	pid_t dprn_pid;			/* pid associated with dprn_dpr */
	char dprn_errmsg[BUFSIZ];	/* error message */
	struct dt_proc_notify *dprn_next; /* next pointer */
} dt_proc_notify_t;

/*
 * An opaque handle to a process, for the external API.
 */
typedef struct dtrace_proc {
	pid_t pid;
} dtrace_proc_t;

/*
 * This is an internal-only flag in the DTRACE_PROC_* space.  External API
 * callers cannot turn it off.
 */
#define DTRACE_PROC_NOTIFIABLE	0x10    /* true if notifiers must be called */

#define	DT_PROC_STOP_IDLE	0x01	/* idle on owner's stop request */
#define	DT_PROC_STOP_CREATE	0x02	/* wait on dpr_cv at process exec */
#define	DT_PROC_STOP_GRAB	0x04	/* wait on dpr_cv at process grab */
#define	DT_PROC_STOP_RESUMING	0x08    /* tell dt_proc_resume() to resume */
#define	DT_PROC_STOP_RESUMED	0x10    /* wait on dpr_cv after resume */
#define	DT_PROC_STOP_PREINIT	0x20	/* wait on dpr_cv at rtld preinit */
#define	DT_PROC_STOP_POSTINIT	0x40	/* wait on dpr_cv at rtld postinit */
#define	DT_PROC_STOP_MAIN	0x80	/* wait on dpr_cv at a.out`main() */

typedef struct dt_proc_hash {
	pthread_mutex_t dph_lock;	/* lock protecting dph_notify list */
	dt_proc_notify_t *dph_notify;	/* list of pending proc notifications */
	dt_list_t dph_lrulist;		/* list of dt_proc_t's in lru order */
	uint_t dph_lrulim;		/* limit on number of procs to hold */
	uint_t dph_lrucnt;		/* count of cached process handles */
	uint_t dph_hashlen;		/* size of hash chains array */
	uint_t dph_noninvasive_created;	/* count of noninvasive -c procs */
	dt_proc_t *dph_hash[1];		/* hash chains array */
} dt_proc_hash_t;

extern pid_t dt_proc_grab_lock(dtrace_hdl_t *dtp, pid_t pid, int flags);
extern void dt_proc_release_unlock(dtrace_hdl_t *, pid_t);
extern void dt_proc_lock(dt_proc_t *dpr);
extern void dt_proc_unlock(dt_proc_t *dpr);
extern dt_proc_t *dt_proc_lookup(dtrace_hdl_t *, pid_t);
extern void dt_proc_enqueue_exits(dtrace_hdl_t *dtp);

/*
 * Proxies for operations in libproc, respecting the execve-retry protocol.
 */
extern int dt_Plookup_by_addr(dtrace_hdl_t *, pid_t, uintptr_t, const char **,
			      GElf_Sym *);
extern const prmap_t *dt_Paddr_to_map(dtrace_hdl_t *, pid_t, uintptr_t);
extern const prmap_t *dt_Plmid_to_map(dtrace_hdl_t *, pid_t, Lmid_t,
    const char *);
extern const prmap_t *dt_Pname_to_map(dtrace_hdl_t *, pid_t, const char *);
extern char *dt_Pobjname(dtrace_hdl_t *, pid_t, uintptr_t, char *, size_t);
extern int dt_Plmid(dtrace_hdl_t *, pid_t, uintptr_t, Lmid_t *);
extern int dt_Pstate(dtrace_hdl_t *, pid_t);
extern int dt_Pxlookup_by_name(dtrace_hdl_t *, pid_t, Lmid_t, const char *,
    const char *, GElf_Sym *, prsyminfo_t *);
extern int dt_Pwritable_mapping(dtrace_hdl_t *, pid_t, uintptr_t);
extern int dt_Psymbol_iter_by_addr(dtrace_hdl_t *, pid_t, const char *, int,
    int, proc_sym_f *, void *);
extern int dt_Pobject_iter(dtrace_hdl_t *, pid_t, proc_map_f *, void *);
extern ssize_t dt_Pread(dtrace_hdl_t *, pid_t, void *, size_t, uintptr_t);

extern void dt_proc_hash_create(dtrace_hdl_t *);
extern void dt_proc_hash_destroy(dtrace_hdl_t *);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_PROC_H */
