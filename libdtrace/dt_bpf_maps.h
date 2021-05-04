/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef _DT_BPF_MAPS_H
#define _DT_BPF_MAPS_H

#ifdef  __cplusplus
extern "C" {
#endif

typedef struct dt_bpf_probe	dt_bpf_probe_t;
struct dt_bpf_probe {
	size_t		prv;		/* probeprov string offset in strtab */
	size_t		mod;		/* probemod string offset in strtab */
	size_t		fun;		/* probefunc string offset in strtab */
	size_t		prb;		/* probename string offset in strtab */
};

#ifdef  __cplusplus
}
#endif

#endif /* _DT_BPF_FUNCS_H */
