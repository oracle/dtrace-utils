/*
 * Oracle Linux DTrace; FUSE logging reimplementation.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef	_RPL_FUSE_LOG_H
#define	_RPL_FUSE_LOG_H

#include <stdarg.h>

/*
 * Reimplementation of fuse_log API in FUSE 3.7.0+.  Not used when FUSE is
 * sufficiently new.
 *
 * We want to use this API if available so that the daemon will log
 * FUSE-level errors to syslog when not running under systemd.  When
 * using older FUSE, this combination will throw away such errors,
 * but that's no excuse for throwing away our own errors too.
 */

enum fuse_log_level
{
	FUSE_LOG_EMERG,
	FUSE_LOG_ALERT,
	FUSE_LOG_CRIT,
	FUSE_LOG_ERR,
	FUSE_LOG_WARNING,
	FUSE_LOG_NOTICE,
	FUSE_LOG_INFO,
	FUSE_LOG_DEBUG
};

typedef void (*rpl_log_func_t)(enum fuse_log_level level, const char *fmt,
			       va_list ap);

void fuse_set_log_func(rpl_log_func_t func);

void fuse_log(enum fuse_log_level level, const char *fmt, ...);

#endif

