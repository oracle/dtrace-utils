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

typedef struct dt_btf		dt_btf_t;
typedef struct bpf_btf_info	btf_info_t;

extern void dt_btf_destroy(dtrace_hdl_t *, dt_btf_t *);
extern dt_btf_t *dt_btf_load_module(dtrace_hdl_t *, dt_module_t *);
extern ctf_dict_t *dt_btf_module_ctf(dtrace_hdl_t *, dt_module_t *);
extern const char *dt_btf_get_string(dtrace_hdl_t *, dt_btf_t *, uint32_t);
extern int32_t dt_btf_lookup_name_kind(dtrace_hdl_t *, dt_btf_t *,
				       const char *, uint32_t);
extern int dt_btf_func_argc(dtrace_hdl_t *dtp, const dt_btf_t *btf,
			    uint32_t id);
extern int dt_btf_func_is_void(dtrace_hdl_t *dtp, const dt_btf_t *btf,
			       uint32_t id);
extern int dt_btf_get_module_ids(dtrace_hdl_t *);
extern int dt_btf_module_fd(const dt_module_t *);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_BTF_H */
