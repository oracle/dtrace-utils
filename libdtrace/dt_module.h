/*
 * Oracle Linux DTrace.
 * Copyright © 2004, 2012, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_MODULE_H
#define	_DT_MODULE_H

#include <dt_impl.h>

#ifdef	__cplusplus
extern "C" {
#endif

extern dt_module_t *dt_module_create(dtrace_hdl_t *, const char *);
extern void dt_module_destroy(dtrace_hdl_t *, dt_module_t *);

extern dt_module_t *dt_module_lookup_by_name(dtrace_hdl_t *, const char *);
extern dt_module_t *dt_module_lookup_by_ctf(dtrace_hdl_t *, ctf_file_t *);

extern ctf_file_t *dt_module_getctf(dtrace_hdl_t *, dt_module_t *);
extern dt_ident_t *dt_module_extern(dtrace_hdl_t *, dt_module_t *,
    const char *, const dtrace_typeinfo_t *);

extern const char *dt_module_modelname(dt_module_t *);

#ifdef	__cplusplus
}
#endif

#endif	/* _DT_MODULE_H */
