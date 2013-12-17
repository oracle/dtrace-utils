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
 * Copyright 2009, 2011, 2012, 2013 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fts.h>
#include <string.h>
#include <dt_kernel_module.h>
#include <dt_string.h>

/*
 * Kernel module list management.  We must maintain bindings from
 * name->filesystem path for all the current kernel's modules, since the system
 * maintains no such list and all mechanisms other than find(1)-analogues have
 * been deprecated or removed in kmod.
 *
 * Note: this code is used by ctf_module_dump as well.
 */

dt_kern_path_t *
dt_kern_path_create(dtrace_hdl_t *dtp, const char *name, const char *path)
{
	uint_t h = dt_strtab_hash(name, NULL) % dtp->dt_kernpathbuckets;
	dt_kern_path_t *dkpp;

	for (dkpp = dtp->dt_kernpaths[h]; dkpp != NULL; dkpp = dkpp->dkp_next) {
		if (strcmp(dkpp->dkp_name, name) == 0)
			return (dkpp);
	}

	if ((dkpp = malloc(sizeof (dt_kern_path_t))) == NULL)
		return (NULL); /* caller must handle allocation failure */

	bzero(dkpp, sizeof (dt_kern_path_t));
	dkpp->dkp_name = strdup(name);
	dkpp->dkp_path = strdup(path);
	dt_list_append(&dtp->dt_kernpathlist, dkpp);
	dkpp->dkp_next = dtp->dt_kernpaths[h];
	dtp->dt_kernpaths[h] = dkpp;
	dtp->dt_nkernpaths++;

	return (dkpp);
}

void
dt_kern_path_destroy(dtrace_hdl_t *dtp, dt_kern_path_t *dkpp)
{
	uint_t h = dt_strtab_hash(dkpp->dkp_name, NULL) % dtp->dt_kernpathbuckets;
	dt_kern_path_t *scan_dkpp;
	dt_kern_path_t *prev_dkpp = NULL;

	dt_list_delete(&dtp->dt_kernpathlist, dkpp);
	assert(dtp->dt_nkernpaths != 0);
	dtp->dt_nkernpaths--;

	for (scan_dkpp = dtp->dt_kernpaths[h]; (scan_dkpp != NULL) &&
		 (scan_dkpp != dkpp); scan_dkpp = scan_dkpp->dkp_next) {
		prev_dkpp = scan_dkpp;
	}
	if (prev_dkpp == NULL)
		dtp->dt_kernpaths[h] = dkpp->dkp_next;
	else
		prev_dkpp->dkp_next = dkpp->dkp_next;

	free(dkpp->dkp_name);
	free(dkpp->dkp_path);
	free(dkpp);
}

/*
 * Construct the mapping of kernel module name -> path for all modules.
 */
int
dt_kern_path_update(dtrace_hdl_t *dtp)
{
	FTS *ftp;
	FTSENT *module;
	char *modpathargs[2] = { dtp->dt_module_path, NULL };

	ftp = fts_open(modpathargs, FTS_PHYSICAL | FTS_NOCHDIR, NULL);

	if (ftp == NULL)
		return errno;

	while ((module = fts_read(ftp)) != NULL) {
		int fterrno;
		char *suffix;
		char modname[PATH_MAX];

		switch (module->fts_info) {
		case FTS_DNR:
		case FTS_ERR:
			fterrno = module->fts_errno;
			fts_close(ftp);
			dt_dprintf("Failed to read kernel module hierarchy "
			    "rooted at %s: %s\n", dtp->dt_module_path,
			    strerror(fterrno));

			return fterrno;
		case FTS_F:
			break;
		default:
			continue;
		}

		suffix = strrstr(module->fts_name, ".ko");
		if ((suffix == NULL) || (suffix[3] != '\0'))
			continue;

		strcpy(modname, module->fts_name);
		suffix = strrstr(modname, ".ko");
		suffix[0] = '\0';

		if (dt_kern_path_create(dtp, modname, module->fts_path) == NULL) {
			fts_close(ftp);
			return EDT_NOMEM;
		}
	}

	/*
	 * fts_read() is unusual in that it sets errno to 0 on a successful
	 * return.
	 */
	if (errno != 0) {
		int error;

		fts_close(ftp);
		error = errno;
		dt_dprintf("Failed to close search for kernel module hierarchy "
		    "rooted at %s/kernel: %s\n", dtp->dt_module_path,
		    strerror(error));

		return error;
	}

	fts_close(ftp);
	return 0;
}

dt_kern_path_t *
dt_kern_path_lookup_by_name(dtrace_hdl_t *dtp, const char *name)
{
	uint_t h = dt_strtab_hash(name, NULL) % dtp->dt_kernpathbuckets;
	dt_kern_path_t *dkpp;

	if (dtp->dt_nkernpaths == 0) {
		int dterrno = dt_kern_path_update(dtp);

		dt_dprintf("Initialized %i kernel module paths, errno %i\n",
		    dtp->dt_nkernpaths, dterrno);

		if (dterrno != 0)
			return (NULL);
	}

	for (dkpp = dtp->dt_kernpaths[h]; dkpp != NULL; dkpp = dkpp->dkp_next) {
		if (strcmp(dkpp->dkp_name, name) == 0)
			return (dkpp);
	}

	return (NULL);
}

/*
 * Public API wrappers.
 */
dt_kern_path_t *
dtrace__internal_kern_path_create(dtrace_hdl_t *dtp, const char *name,
                                 const char *path)
    __attribute__((alias("dt_kern_path_create")));

void
dtrace__internal_kern_path_destroy(dtrace_hdl_t *dtp, dt_kern_path_t *dkpp)
    __attribute__((alias("dt_kern_path_destroy")));

int
dtrace__internal_kern_path_update(dtrace_hdl_t *dtp)
    __attribute__((alias("dt_kern_path_update")));

dt_kern_path_t *
dtrace__internal_kern_path_lookup_by_name(dtrace_hdl_t *dtp, const char *name)
    __attribute__((alias("dt_kern_path_lookup_by_name")));
