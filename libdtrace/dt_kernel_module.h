/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2012, 2013, 2014 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
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

/*
 * Public API wrappers.
 *
 * Programs outside libdtrace can call these -- but their API stability
 * is not guaranteed, and they should only be called by programs inside
 * the DTrace package.
 */

extern dt_kern_path_t *
dtrace__internal_kern_path_create(dtrace_hdl_t *dtp, char *name, char *path);

extern void
dtrace__internal_kern_path_destroy(dtrace_hdl_t *dtp, dt_kern_path_t *dkpp);

extern int
dtrace__internal_kern_path_update(dtrace_hdl_t *dtp);

extern dt_kern_path_t *
dtrace__internal_kern_path_lookup_by_name(dtrace_hdl_t *dtp, const char *name);

#endif	/* _DT_KERNEL_MODULE_H */
