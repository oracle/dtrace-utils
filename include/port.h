/*
 * Oracle Linux DTrace.
 * Copyright (c) 2011, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef _PORT_H
#define _PORT_H

#include <pthread.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/compiler.h>
#include <sys/types.h>
#include <sys/dtrace_types.h>
#include <sys/ptrace.h>
#include <config.h>

extern size_t strlcpy(char *, const char *, size_t);
extern size_t strlcat(char *, const char *, size_t);

extern int gmatch(const char *s, const char *p);

hrtime_t gethrtime(void);

int p_online(int cpun);

#define MUTEX_HELD(x)	((x)->__data.__count == 0)

int daemonize(int close_fds);
int systemd_notify(const char *message);

_dt_noreturn_ void daemon_perr(int fd, const char *err, int err_no);
_dt_printflike_(2, 3) void daemon_log(int fd, const char *fmt, ...);
void daemon_vlog(int fd, const char *fmt, va_list ap);

unsigned long linux_version_code(void);

#ifndef HAVE_ELF_GETSHDRSTRNDX
#define elf_getshdrstrndx elf_getshstrndx
#define elf_getshdrnum elf_getshnum
#endif

#ifndef HAVE_CLOSE_RANGE
int close_range(unsigned int first, unsigned int last, unsigned int flags);
#endif

#ifndef HAVE_GETTID
pid_t gettid(void);
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

/*
 * Machine and relocation types for BPF (only available in newer glibc headers).
 */
#ifndef EM_BPF
#define EM_BPF			247	/* Linux BPF -- in-kernel virtual machine */
#endif
#ifndef R_BPF_NONE
#define R_BPF_NONE		0	/* No reloc */
#endif
#ifndef R_BPF_64_64
#define R_BPF_64_64		1
#endif
#ifndef R_BPF_64_32
#define R_BPF_64_32		10
#endif

/*
 * Error code returned by the uprobes kernel facility (only available in kernel
 * headers that are not part of the UAPI).
 */
#ifndef ENOTSUPP
#define ENOTSUPP		524	/* Operation is not supported */
#endif

#endif
