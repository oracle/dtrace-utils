/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_BPF_H
#define	_DT_BPF_H

#include <linux/perf_event.h>
#include <dt_impl.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define DT_CONST_EPID	1
#define DT_CONST_ARGC	2
#define DT_CONST_PRID	3

extern int perf_event_open(struct perf_event_attr *attr, pid_t pid, int cpu,
			   int group_fd, unsigned long flags);
extern int bpf(enum bpf_cmd cmd, union bpf_attr *attr);

extern int dt_bpf_gmap_create(dtrace_hdl_t *);
extern int dt_bpf_map_update(int fd, const void *key, const void *val);
extern int dt_bpf_load_progs(dtrace_hdl_t *, uint_t);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_BPF_H */
