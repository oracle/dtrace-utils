/*
 * Oracle Linux DTrace.
 * Copyright (c) 2008, 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <link.h>
#include <sys/dtrace.h>
#include <sys/compiler.h>
#include <sys/ioctl.h>
#include <pthread.h>

#include <gelf.h>

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/*
 * Users may set the following environment variables to affect the way
 * helper initialization takes place:
 *
 *	DTRACE_DOF_INIT_DEBUG		enable debugging output
 *	DTRACE_DOF_INIT_DISABLE		disable helper loading
 *	DTRACE_DOF_INIT_DEVNAME		set the path to the helper node
 */

static const char *devname = "/dev/dtrace/helper";

static int gen;			/* DOF helper generation */
static boolean_t dof_init_debug = B_FALSE;	/* From DTRACE_DOF_INIT_DEBUG */

static char *errmsg_open, *errmsg_ioctl_failed, *errmsg_ioctl_ok;

static void dtrace_dof_register(void);

static int
private_pthread_atfork(void (*prepare)(void), void (*parent)(void),
		       void (*child)(void))
{
  /* Sufficiently old glibc doesn't define pthread_atfork in libc, so we have to
     use an internal interface instead in order to not force all probe users to
     pull in -lpthread.  This internal interface is used by the pthread_atfork
     implementation in libc_nonshared.a in all glibcs new enough not to be
     affected by this problem, so there are no stable-ABI concerns here: the ABI
     is stable regardless.  */

#ifdef HAVE_PTHREAD_ATFORK
	return pthread_atfork(prepare, parent, child);
#else
	extern int __register_atfork(void (*prepare) (void),
				     void (*parent) (void),
				     void (*child) (void), void *dso_handle);
	extern void *__dso_handle _dt_weak_;

	return __register_atfork(prepare, parent, child, __dso_handle);
#endif
}

_dt_constructor_(dtrace_dof_init)
static void
dtrace_dof_init(void)
{
	const char *p;

	if (getenv("DTRACE_DOF_INIT_DISABLE") != NULL)
		return;

	if (getenv("DTRACE_DOF_INIT_DEBUG") != NULL)
		dof_init_debug = B_TRUE;

	if ((p = getenv("DTRACE_DOF_INIT_DEVNAME")) != NULL)
		devname = p;

	/*
	 * Prep error messages to avoid non-async-signal-safe printfs inside
	 * dtrace_dof_register().
	 */
	if (asprintf(&errmsg_ioctl_failed, "DRTI: Ioctl failed\n") < 0)
		errmsg_ioctl_failed = NULL;

	if (dof_init_debug) {
		if (asprintf(&errmsg_open, "DRTI: Failed to open helper device %s\n",
			     devname) < 0)
			errmsg_open = NULL;

		if (asprintf(&errmsg_ioctl_ok, "DRTI: Ioctl OK (gen %d)\n",
			     gen) < 0)
			errmsg_ioctl_ok = NULL;
	}

	dtrace_dof_register();
	private_pthread_atfork(NULL, NULL, dtrace_dof_register);
}

static void
dtrace_dof_register(void)
{
	int fd;

	if ((fd = open(devname, O_RDWR, O_CLOEXEC)) < 0) {
		if (dof_init_debug && errmsg_open)
			write(2, errmsg_open, strlen(errmsg_open));
		return;
	}

	gen = ioctl(fd, DTRACEHIOC_HASUSDT, (uintptr_t)dtrace_dof_init);
	if (gen == -1) {
		if (errmsg_ioctl_failed)
			write(2, errmsg_ioctl_failed,
			      strlen(errmsg_ioctl_failed));
		else {
			const char *ioctl_failed = "DRTI: Ioctl failed for DOF\n";
			write(2, ioctl_failed, strlen(ioctl_failed));
		}
	} else if (dof_init_debug && errmsg_ioctl_ok)
		write(2, errmsg_ioctl_ok, strlen(errmsg_ioctl_ok));

	close(fd);
}

_dt_destructor_(dtrace_dof_fini)
static void
dtrace_dof_fini(void)
{
	int fd;

	if ((fd = open(devname, O_RDWR | O_CLOEXEC)) < 0) {
		if (dof_init_debug)
			dprintf(2, "DRTI: Failed to open helper device %s\n",
				devname);
		return;
	}

	if ((gen = ioctl(fd, DTRACEHIOC_REMOVE, gen)) == -1)
		dprintf(2, "DRTI: Ioctl failed to remove DOF (gen %d)\n", gen);
	else if (dof_init_debug)
		dprintf(2, "DRTI: Ioctl removed DOF (gen %d)\n", gen);

	close(fd);

	free(errmsg_open);
	free(errmsg_ioctl_failed);
	free(errmsg_ioctl_ok);
	errmsg_open = NULL;
	errmsg_ioctl_failed = NULL;
	errmsg_ioctl_ok = NULL;
}
