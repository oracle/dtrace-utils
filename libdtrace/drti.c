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
 * Copyright 2008, 2013 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
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
 * In Solaris 10 GA, the only mechanism for communicating helper information
 * is through the DTrace helper pseudo-device node in /devices; there is
 * no /dev link. Because of this, USDT providers and helper actions don't
 * work inside of non-global zones. This issue was addressed by adding
 * the /dev and having this initialization code use that /dev link. If the
 * /dev link doesn't exist it falls back to looking for the /devices node
 * as this code may be embedded in a binary which runs on Solaris 10 GA.
 *
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
#if 1
	char mfn[PATH_MAX];		/* "/proc/<pid>/maps" */
	char str[4096];			/* read buffer */
	FILE *fp;
	struct link_map fmap = { 0x0, };
#endif

	if (getenv("DTRACE_DOF_INIT_DISABLE") != NULL)
		return;

	if (getenv("DTRACE_DOF_INIT_DEBUG") != NULL)
		dof_init_debug = B_TRUE;

	if ((p = getenv("DTRACE_DOF_INIT_DEVNAME")) != NULL)
		devname = p;

	if ((fd = open(devname, O_RDWR)) < 0) {
		if (dof_init_debug)
			dprintf(1, "DRTI: Failed to open helper device %s\n",
				devname);
		return;
	}

#if 0
	if (dlinfo(RTLD_SELF, RTLD_DI_LINKMAP, &lmp) == -1 || lmp == NULL) {
		dprintf(1, "DRTI: Couldn't discover module name or address.\n");
                goto out;
	}

	if (dlinfo(RTLD_SELF, RTLD_DI_LMID, &lmid) == -1) {
		dprintf(1, "DRTI: Couldn't discover link map ID.\n");
                goto out;
	}
#else
	lmid = 0;			/* We need a way to determine this. */

	snprintf(mfn, sizeof(mfn), "/proc/%d/maps", getpid());
	if ((fp = fopen(mfn, "re")) == NULL) {
		dprintf(1, "DRTI: Failed to open maps file.\n");
                goto out;
	}
	while (fgets(str, sizeof(str), fp) != NULL) {
		uintptr_t	start, end;
		char		*p = str, *q;

		start = strtol(p, &p, 16);
		if (*p != '-')
			continue;

		end = strtol(++p, &p, 16);

		if (start > (uintptr_t)dtrace_dof_init ||
		    (uintptr_t)dtrace_dof_init > end)
			continue;

		if ((p = strrchr(str, ' ')) == NULL)
			continue;
		if ((q = strchr(p, '\n')) != NULL)
			*q = '\0';

		fmap.l_addr = start;
		fmap.l_name = p + 1;
		lmp = &fmap;

		break;
	}
	fclose(fp);
#endif

	if ((modname = strrchr(lmp->l_name, '/')) == NULL)
		modname = lmp->l_name;
	else
		modname++;

	if (dof->dofh_ident[DOF_ID_MAG0] != DOF_MAG_MAG0 ||
	    dof->dofh_ident[DOF_ID_MAG1] != DOF_MAG_MAG1 ||
	    dof->dofh_ident[DOF_ID_MAG2] != DOF_MAG_MAG2 ||
	    dof->dofh_ident[DOF_ID_MAG3] != DOF_MAG_MAG3) {
		dprintf(1, "DRTI: .SUNW_dof section corrupt in %s.\n",
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
		dprintf(1, "DRTI: Ioctl failed for DOF at %p\n", (void *)dof);
	else if (dof_init_debug)
		dprintf(1, "DRTI: Ioctl OK for DOF at %p (gen %d)\n",
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
			dprintf(1, "DRTI: Failed to open helper device %s\n",
				devname);
		return;
	}

	if ((gen = ioctl(fd, DTRACEHIOC_REMOVE, gen)) == -1)
		dprintf(1, "DRTI: Ioctl failed to remove DOF (gen %d)\n", gen);
	else if (dof_init_debug)
		dprintf(1, "DRTI: Ioctl removed DOF (gen %d)\n", gen);

	close(fd);
}
