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
 * Copyright 2011 -- 2016 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _PORT_H
#define _PORT_H

#include <pthread.h>
#include <mutex.h>
#include <sys/types.h>
#include <sys/dtrace_types.h>
#include <sys/ptrace.h>
#include <config.h>

extern size_t strlcpy(char *, const char *, size_t);
extern size_t strlcat(char *, const char *, size_t);

extern int gmatch(const char *s, const char *p);

hrtime_t gethrtime(void);

int p_online(int cpun);

int mutex_init(mutex_t *m, int flags1, void *ptr);

unsigned long linux_version_code(void);

#ifndef HAVE_ELF_GETSHDRSTRNDX
#define elf_getshdrstrndx elf_getshstrndx
#define elf_getshdrnum elf_getshnum
#endif

#ifndef HAVE_WAITFD
int waitfd(int which, pid_t upid, int options, int flags);
#endif

/*
 * New open() flags not supported in OL6 glibc.
 */
#ifndef O_PATH
#define O_PATH          010000000
#endif

/*
 * New ptrace requests not widely supported in glibc headers.
 */
#ifndef PTRACE_SEIZE
#define PTRACE_SEIZE		0x4206
#endif
#ifndef PTRACE_INTERRUPT
#define PTRACE_INTERRUPT	0x4207
#endif
#ifndef PTRACE_LISTEN
#define PTRACE_LISTEN		0x4208
#endif
#ifndef PTRACE_GETMAPFD
#define PTRACE_GETMAPFD		0x42A5
#endif
#ifndef PTRACE_EVENT_STOP
#define PTRACE_EVENT_STOP       128
#endif

#endif
