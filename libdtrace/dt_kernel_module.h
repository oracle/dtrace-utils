/*
 * Oracle Linux DTrace.
 * Copyright (c) 2012, 2014, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_DT_KERNEL_MODULE_H
#define	_DT_KERNEL_MODULE_H

#include <dt_impl.h>

extern dt_kern_path_t *dt_kern_path_create(dtrace_hdl_t *dtp, char *name,
    char *path);
extern int dt_kern_path_update(dtrace_hdl_t *dtp);
extern dt_kern_path_t *dt_kern_path_lookup_by_name(dtrace_hdl_t *dtp,
    const char *name);
extern void dt_kern_path_destroy(dtrace_hdl_t *dtp, dt_kern_path_t *dkpp);

#endif	/* _DT_KERNEL_MODULE_H */
