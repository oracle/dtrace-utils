/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_BPF_H
#define	_DT_BPF_H

#include <sys/dtrace_types.h>
#include <linux/bpf.h>
#include <linux/perf_event.h>

struct dtrace_hdl;

#ifdef	__cplusplus
extern "C" {
#endif

#define DT_CONST_EPID	1
#define DT_CONST_PRID	2
#define DT_CONST_CLID	3
#define DT_CONST_ARGC	4
#define DT_CONST_STBSZ	5
#define DT_CONST_STKOFF	6
#define DT_CONST_STKSIZ	7

extern int perf_event_open(struct perf_event_attr *attr, pid_t pid, int cpu,
			   int group_fd, unsigned long flags);
extern int bpf(enum bpf_cmd cmd, union bpf_attr *attr);

extern int dt_bpf_gmap_create(struct dtrace_hdl *);
extern int dt_bpf_map_lookup(int fd, const void *key, void *val);
extern int dt_bpf_map_update(int fd, const void *key, const void *val);
extern int dt_bpf_load_progs(struct dtrace_hdl *, uint_t);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_BPF_H */
