/*
 * Oracle Linux DTrace.
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
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
