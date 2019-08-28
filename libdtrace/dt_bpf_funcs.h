/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_BPF_FUNCS_H
#define	_DT_BPF_FUNCS_H

#ifdef	__cplusplus
extern "C" {
#endif

#define DT_BPF_GET_GVAR			-1
#define DT_BPF_SET_GVAR			-2
#define DT_BPF_GET_TVAR			-3
#define DT_BPF_SET_TVAR			-4
#define DT_BPF_GET_STRING		-5

extern const char *dt_bpf_fname(int id);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_BPF_FUNCS_H */
