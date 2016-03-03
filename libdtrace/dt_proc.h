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
 * Copyright 2007, 2011 -- 2014 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
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
	uint8_t dpr_usdt;		/* usdt flag: usdt initialized */
	uint8_t dpr_created;            /* proc flag: true if we created this
					   process, false if we grabbed it */
	uint8_t dpr_monitoring;		/* true if we should background-monitor
					   the process right now */

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
	 * proxy_reattach() and proxy_monitor().
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
	} dpr_proxy_args;
} dt_proc_t;

typedef struct dt_proc_notify {
	dt_proc_t *dprn_dpr;		/* process associated with the event */
	char dprn_errmsg[BUFSIZ];	/* error message */
	struct dt_proc_notify *dprn_next; /* next pointer */
} dt_proc_notify_t;

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
	pthread_cond_t dph_cv;		/* cond for waiting for dph_notify */
	dt_proc_notify_t *dph_notify;	/* list of pending proc notifications */
	dt_list_t dph_lrulist;		/* list of dt_proc_t's in lru order */
	uint_t dph_lrulim;		/* limit on number of procs to hold */
	uint_t dph_lrucnt;		/* count of cached process handles */
	uint_t dph_hashlen;		/* size of hash chains array */
	dt_proc_t *dph_hash[1];		/* hash chains array */
} dt_proc_hash_t;

extern struct dtrace_prochandle dt_proc_create(dtrace_hdl_t *,
    const char *, char *const *, int flags);

extern struct dtrace_prochandle dt_proc_grab(dtrace_hdl_t *, pid_t, int flags);
extern void dt_proc_release(dtrace_hdl_t *, struct dtrace_prochandle *);
extern long dt_proc_continue(dtrace_hdl_t *, struct dtrace_prochandle *);
extern void dt_proc_lock(dtrace_hdl_t *, struct dtrace_prochandle *);
extern void dt_proc_unlock(dtrace_hdl_t *, struct dtrace_prochandle *);
extern dt_proc_t *dt_proc_lookup(dtrace_hdl_t *, struct dtrace_prochandle *, int);

/*
 * Proxies for operations in libproc, respecting the execve-retry protocol.
 */
extern int dt_Plookup_by_addr(dtrace_hdl_t *, struct dtrace_prochandle *,
    uintptr_t, char *, size_t, GElf_Sym *);
extern const prmap_t *dt_Paddr_to_map(dtrace_hdl_t *, struct dtrace_prochandle *,
    uintptr_t);
extern const prmap_t *dt_Plmid_to_map(dtrace_hdl_t *, struct dtrace_prochandle *,
    Lmid_t, const char *);
extern const prmap_t *dt_Pname_to_map(dtrace_hdl_t *, struct dtrace_prochandle *,
    const char *);
extern char *dt_Pobjname(dtrace_hdl_t *, struct dtrace_prochandle *,
    uintptr_t, char *, size_t);
extern int dt_Plmid(dtrace_hdl_t *, struct dtrace_prochandle *, uintptr_t,
    Lmid_t *);
extern int dt_Pxlookup_by_name(dtrace_hdl_t *, struct dtrace_prochandle *,
    Lmid_t, const char *, const char *, GElf_Sym *, prsyminfo_t *);
extern int dt_Pwritable_mapping(dtrace_hdl_t *, struct dtrace_prochandle *,
    uintptr_t);
extern int dt_Psymbol_iter_by_addr(dtrace_hdl_t *, struct dtrace_prochandle *,
    const char *, int, int, proc_sym_f *, void *);
extern int dt_Pobject_iter(dtrace_hdl_t *, struct dtrace_prochandle *,
    proc_map_f *, void *);

extern void dt_proc_hash_create(dtrace_hdl_t *);
extern void dt_proc_hash_destroy(dtrace_hdl_t *);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_PROC_H */
