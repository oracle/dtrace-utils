/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
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
 * Copyright 2013 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_DT_DEBUG_H
#define	_DT_DEBUG_H

#include <sys/compiler.h>
#include <stdarg.h>

extern int _dtrace_debug;		/* debugging messages enabled */

_dt_printflike_(1,2)
extern void dt_dprintf(const char *, ...);
extern void dt_debug_printf(const char *subsys, const char *format,
    va_list alist);
extern void dt_debug_dump(int);

#endif
