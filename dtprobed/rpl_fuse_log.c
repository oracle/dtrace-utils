/*
 * Oracle Linux DTrace; FUSE logging reimplementation.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <sys/compiler.h>
#include "rpl_fuse_log.h"
#include <stdarg.h>
#include <stdio.h>

static void default_log_func(enum fuse_log_level level _dt_unused_,
			     const char *fmt, va_list ap)
{
	vfprintf(stderr, fmt, ap);
}

static rpl_log_func_t log_func = default_log_func;

void fuse_set_log_func(rpl_log_func_t func)
{
	log_func = func;
}

void fuse_log(enum fuse_log_level level, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	log_func(level, fmt, ap);
	va_end(ap);
}
