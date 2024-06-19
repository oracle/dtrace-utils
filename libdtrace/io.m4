/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * This file is an m4 script which is first preprocessed by cpp or cc -E to
 * substitute #define tokens for their values. It is then run over io.d.in
 * to replace those tokens with their values to create the finished io.d.
 */

#include <linux/kconfig.h>
#include <linux/buffer_head.h>
#include <sys/file.h>
#if 0
#ifndef __USE_UNIX98
# define __USE_UNIX98
#endif
#ifndef __USE_XOPEN2K8
# define __USE_XOPEN2K8
#endif
#endif
#include <sys/fcntl.h>

#define	DEF_REPLACE(x)	__def_replace(#x,x)

DEF_REPLACE(O_ACCMODE)
DEF_REPLACE(O_RDONLY)
DEF_REPLACE(O_WRONLY)
DEF_REPLACE(O_RDWR)
DEF_REPLACE(O_CREAT)
DEF_REPLACE(O_EXCL)
DEF_REPLACE(O_NOCTTY)
DEF_REPLACE(O_TRUNC)
DEF_REPLACE(O_APPEND)
DEF_REPLACE(O_NONBLOCK)
DEF_REPLACE(O_NDELAY)
DEF_REPLACE(O_SYNC)
DEF_REPLACE(O_ASYNC)
DEF_REPLACE(O_DIRECTORY)
DEF_REPLACE(O_NOFOLLOW)
DEF_REPLACE(O_CLOEXEC)
DEF_REPLACE(O_DSYNC)
DEF_REPLACE(O_RSYNC)
#ifdef BD_PARTNO
DEF_REPLACE(BD_PARTNO)
#endif
#include "io.platform.m4"
