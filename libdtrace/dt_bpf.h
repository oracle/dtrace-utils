/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_BPF_H
#define	_DT_BPF_H

#include <sys/dtrace_types.h>
#include <linux/bpf.h>
#include <linux/perf_event.h>
#include <dtrace/difo.h>
#include <dt_impl.h>

struct dtrace_hdl;

#ifdef	__cplusplus
extern "C" {
#endif

#define DT_CONST_EPID			1
#define DT_CONST_PRID			2
#define DT_CONST_CLID			3
#define DT_CONST_ARGC			4
#define DT_CONST_STBSZ			5
#define DT_CONST_STRSZ			6
#define DT_CONST_STKSIZ			7
#define DT_CONST_BOOTTM			8
#define DT_CONST_NSPEC			9
#define DT_CONST_NCPUS			10
#define DT_CONST_PC			11
#define DT_CONST_TUPSZ			12
#define DT_CONST_TASK_PID		13
#define DT_CONST_TASK_TGID		14
#define DT_CONST_TASK_REAL_PARENT	15
#define DT_CONST_TASK_COMM		16
#define DT_CONST_MUTEX_OWNER		17
#define DT_CONST_RWLOCK_CNTS		18
#define DT_CONST_DCTX_RODATA		19
#define DT_CONST_RODATA_OFF		20
#define DT_CONST_RODATA_SIZE		21
#define DT_CONST_ZERO_OFF		22
#define DT_CONST_STACK_OFF		23

#define DT_BPF_LOG_SIZE_DEFAULT	(UINT32_MAX >> 8)
#define DT_BPF_LOG_SIZE_SMALL	4096

extern int dt_perf_event_open(struct perf_event_attr *attr, pid_t pid, int cpu,
			      int group_fd, unsigned long flags);
extern int dt_bpf(enum bpf_cmd cmd, union bpf_attr *attr);

extern int dt_bpf_gmap_create(struct dtrace_hdl *);
extern int dt_bpf_lockmem_error(struct dtrace_hdl *dtp, const char *msg);

extern int dt_bpf_map_lookup(int fd, const void *key, void *val);
extern int dt_bpf_map_next_key(int fd, const void *key, void *nxt);
extern int dt_bpf_map_update(int fd, const void *key, const void *val);
extern int dt_bpf_map_delete(int fd, const void *key);
extern int dt_bpf_map_get_fd_by_id(uint32_t id);
extern int dt_bpf_map_lookup_fd(int fd, const void *okey);
extern int dt_bpf_map_lookup_inner(int fd, const void *okey, const void *ikey,
				   void *val);
extern int dt_bpf_map_update_inner(int fd, const void *okey, const void *ikey,
				   const void *val);
extern int dt_bpf_prog_load(enum bpf_prog_type prog_type,
			    const dtrace_difo_t *dp, uint32_t log_level,
			    char *log_buf, size_t log_buf_sz);
extern int dt_bpf_raw_tracepoint_open(const void *tp, int fd);
extern int dt_bpf_make_progs(struct dtrace_hdl *, uint_t);
extern int dt_bpf_load_progs(struct dtrace_hdl *, uint_t);
extern void dt_bpf_init_helpers(struct dtrace_hdl *dtp);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_BPF_H */
