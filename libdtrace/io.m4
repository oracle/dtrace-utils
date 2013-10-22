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
 * Copyright 2005, 2012, 2013 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * This file is an m4 script which is first preprocessed by cpp or cc -E to
 * substitute #define tokens for their values. It is then run over io.d.in
 * to replace those tokens with their values to create the finished io.d.
 */

/* #include <linux/buffer_head.h> */
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
#include "io.platform.m4"
