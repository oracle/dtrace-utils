/*
 * Oracle Linux DTrace.
 * Copyright (c) 2008, 2024, Oracle and/or its affiliates. All rights reserved.
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

static const char *modname;	/* Name of this load object */
static int gen;			/* DOF helper generation */
extern dof_hdr_t __SUNW_dof;	/* DOF defined in the .SUNW_dof section */
static boolean_t dof_init_debug = B_FALSE;	/* From DTRACE_DOF_INIT_DEBUG */
static dof_helper_t dh;

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
	dof_hdr_t *dof = &__SUNW_dof;
#ifdef _LP64
	Elf64_Ehdr *elf;
#else
	Elf32_Ehdr *elf;
#endif
	struct link_map *lmp = NULL;
	Lmid_t lmid = -1;
	const char *p;
	char str[4096];			/* read buffer */
	char *enm = NULL;		/* pointer to target executable name */
	FILE *fp;
	struct link_map fmap = { 0x0, };

	if (getenv("DTRACE_DOF_INIT_DISABLE") != NULL)
		return;

	if (getenv("DTRACE_DOF_INIT_DEBUG") != NULL)
		dof_init_debug = B_TRUE;

	if ((p = getenv("DTRACE_DOF_INIT_DEVNAME")) != NULL)
		devname = p;

#if 0
	if (dlinfo(RTLD_SELF, RTLD_DI_LINKMAP, &lmp) == -1) {
		dprintf(2, "DRTI: Couldn't discover module name or address.\n");
		return;
	}

	if (dlinfo(RTLD_SELF, RTLD_DI_LMID, &lmid) == -1) {
		dprintf(2, "DRTI: Couldn't discover link map ID.\n");
		return;
	}
#else
	lmid = 0;			/* We need a way to determine this. */

	if ((fp = fopen("/proc/self/maps", "re")) == NULL) {
		dprintf(2, "DRTI: Failed to open maps file.\n");
		return;
	}
	while (fgets(str, sizeof(str), fp) != NULL) {
		uintptr_t	start, end;
		char		*p = str, *q;

		start = strtoul(p, &p, 16);
		if (*p != '-')
			continue;

		end = strtoul(++p, &p, 16);

		if (start > (uintptr_t)dtrace_dof_init ||
		    (uintptr_t)dtrace_dof_init > end)
			continue;

		if ((p = strrchr(str, ' ')) == NULL)
			continue;
		p++;  /* move past the leading space char */
		if ((q = strchr(p, '\n')) != NULL)
			*q = '\0';
		enm = strdup(p); /* Save name of object w/ dtrace_dof_init */
		break;
	}

	if (_dt_unlikely_(enm == NULL)) {
		fclose(fp);
		dprintf(2, "DRTI: Couldn't discover module name or address.\n");
		return;
	}

	/* Now start at the beginning & look for 1st segment of the target */
	rewind(fp);
	while (fgets(str, sizeof(str), fp) != NULL) {
		uintptr_t	start;
		char		*p = str, *q;

		start = strtoul(p, &p, 16);
		if (*p != '-')
			continue;
		if ((p = strrchr(str, ' ')) == NULL)
			continue;
		p++;  /* move past the leading space char */
		if ((q = strchr(p, '\n')) != NULL)
			*q = '\0';

		/* If found the 1st segment of the target executable */
		if (strcmp(enm, p) == 0) {
			fmap.l_addr = start;  /* record start address */
			fmap.l_name = p;
			lmp = &fmap;
			break;
		}
	}
	free(enm);
	fclose(fp);
#endif
	if (_dt_unlikely_(lmp == NULL)) {
		dprintf(2, "DRTI: Couldn't discover module name or address.\n");
		return;
	}

	if ((modname = strrchr(lmp->l_name, '/')) == NULL)
		modname = lmp->l_name;
	else
		modname++;

	if (dof->dofh_ident[DOF_ID_MAG0] != DOF_MAG_MAG0 ||
	    dof->dofh_ident[DOF_ID_MAG1] != DOF_MAG_MAG1 ||
	    dof->dofh_ident[DOF_ID_MAG2] != DOF_MAG_MAG2 ||
	    dof->dofh_ident[DOF_ID_MAG3] != DOF_MAG_MAG3) {
		dprintf(2, "DRTI: .SUNW_dof section corrupt in %s.\n",
			lmp->l_name);
		return;
	}

	elf = (void *)lmp->l_addr;

	dh.dofhp_dof = (uintptr_t)dof;
	dh.dofhp_addr = elf->e_type == ET_DYN ? lmp->l_addr : 0;

	if (lmid == 0) {
		(void) snprintf(dh.dofhp_mod, sizeof (dh.dofhp_mod),
		    "%s", modname);
	} else {
		(void) snprintf(dh.dofhp_mod, sizeof (dh.dofhp_mod),
		    "LM%lu`%s", lmid, modname);
	}

	/*
	 * Prep error messages to avoid non-async-signal-safe printfs inside
	 * dtrace_dof_register().
	 */
	if (asprintf(&errmsg_ioctl_failed, "DRTI: Ioctl failed for DOF at %llx\n",
		     (long long unsigned) dh.dofhp_addr) < 0)
		errmsg_ioctl_failed = NULL;

	if (dof_init_debug) {
		if (asprintf(&errmsg_open, "DRTI: Failed to open helper device %s\n",
			     devname) < 0)
			errmsg_open = NULL;

		if (asprintf(&errmsg_ioctl_ok, "DRTI: Ioctl OK for DOF at %llx (gen %d)\n",
			     (long long unsigned) dh.dofhp_addr, gen) < 0)
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

	if ((gen = ioctl(fd, DTRACEHIOC_ADDDOF, &dh)) == -1) {
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
