/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_BTF_H
#define	_DT_BTF_H

#include <dt_impl.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct dt_btf	dt_btf_t;

extern void dt_btf_destroy(dtrace_hdl_t *, dt_btf_t *);
extern dt_btf_t *dt_btf_load_module(dtrace_hdl_t *, dt_module_t *);
extern ctf_dict_t *dt_btf_module_ctf(dtrace_hdl_t *, dt_module_t *);
extern const char *dt_btf_get_string(dtrace_hdl_t *, dt_btf_t *, uint32_t);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_BTF_H */
