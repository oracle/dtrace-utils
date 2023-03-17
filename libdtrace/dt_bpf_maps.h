/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef _DT_BPF_MAPS_H
#define _DT_BPF_MAPS_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <dtrace/conf.h>

typedef struct dt_bpf_probe	dt_bpf_probe_t;
struct dt_bpf_probe {
	size_t		prv;		/* probeprov string offset in strtab */
	size_t		mod;		/* probemod string offset in strtab */
	size_t		fun;		/* probefunc string offset in strtab */
	size_t		prb;		/* probename string offset in strtab */
};

typedef struct dt_bpf_specs	dt_bpf_specs_t;
struct dt_bpf_specs {
	uint64_t	written;	/* number of spec buffers written */
	uint32_t	draining;	/* 1 if userspace has been asked to
					 * drain this buffer */
};

typedef struct dt_bpf_cpuinfo	dt_bpf_cpuinfo_t;
struct dt_bpf_cpuinfo {
	cpuinfo_t	ci;
	uint64_t	buf_drops;
	uint64_t	agg_drops;
};

#ifdef  __cplusplus
}
#endif

#endif /* _DT_BPF_FUNCS_H */
