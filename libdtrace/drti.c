/*
 * Oracle Linux DTrace.
 * Copyright (c) 2008, 2023, Oracle and/or its affiliates. All rights reserved.
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
	dof_helper_t dh;
	struct link_map *lmp = NULL;
	Lmid_t lmid = -1;
	int fd;
	const char *p;
	char mfn[PATH_MAX];		/* "/proc/<pid>/maps" */
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

	if ((fd = open(devname, O_RDWR)) < 0) {
		if (dof_init_debug)
			dprintf(2, "DRTI: Failed to open helper device %s\n",
				devname);
		return;
	}

#if 0
	if (dlinfo(RTLD_SELF, RTLD_DI_LINKMAP, &lmp) == -1) {
		dprintf(2, "DRTI: Couldn't discover module name or address.\n");
                goto out;
	}

	if (dlinfo(RTLD_SELF, RTLD_DI_LMID, &lmid) == -1) {
		dprintf(2, "DRTI: Couldn't discover link map ID.\n");
                goto out;
	}
#else
	lmid = 0;			/* We need a way to determine this. */

	snprintf(mfn, sizeof(mfn), "/proc/self/maps");
	if ((fp = fopen(mfn, "re")) == NULL) {
		dprintf(2, "DRTI: Failed to open maps file.\n");
                goto out;
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
		goto out;
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
                goto out;
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
                goto out;
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

	if ((gen = ioctl(fd, DTRACEHIOC_ADDDOF, &dh)) == -1)
		dprintf(2, "DRTI: Ioctl failed for DOF at %p\n", (void *)dof);
	else if (dof_init_debug)
		dprintf(2, "DRTI: Ioctl OK for DOF at %p (gen %d)\n",
			(void *)dof, gen);

 out:
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
}
