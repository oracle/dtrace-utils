/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_BPF_H
#define	_DT_BPF_H

#include <dt_impl.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define DT_CONST_EPID	1
#define DT_CONST_ARGC	2

extern int dt_bpf_gmap_create(dtrace_hdl_t *, uint_t);
extern int dt_bpf_map_update(int fd, const void *key, const void *val);
extern int dt_bpf_prog(dtrace_hdl_t *, dtrace_prog_t *);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_BPF_H */
