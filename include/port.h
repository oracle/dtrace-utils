/*
 * Oracle Linux DTrace.
 * Copyright (c) 2011, 2016, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
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

#ifndef __NR_pidfd_open
#define __NR_pidfd_open 434
#endif

#ifndef HAVE_PIDFD_OPEN
int pidfd_open(pid_t pid, unsigned int flags);
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
