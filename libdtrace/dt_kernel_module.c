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
 * Copyright 2009, 2011, 2012, 2013, 2014 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Kernel module list management.  We must maintain bindings from
 * name->filesystem path for all the current kernel's modules, since the system
 * maintains no such list and all mechanisms other than find(1)-analogues have
 * been deprecated or removed in kmod.
 */

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <ftw.h>
#include <pthread.h>
#include <string.h>

#include <dt_kernel_module.h>
#include <dt_string.h>
#include <port.h>

static pthread_mutex_t kern_path_update_lock = PTHREAD_MUTEX_INITIALIZER;
static dtrace_hdl_t *locked_dtp;

static int dt_kern_path_update_one_entry(const char *fpath, const struct stat *sb,
    int typeflag, struct FTW *ftwbuf);

/*
 * On successful return, name and path become owned by dt_kern_path_create()
 * (and may be freed immediately).
 *
 * Note: this code is used by ctf_module_dump as well.
 */

dt_kern_path_t *
dt_kern_path_create(dtrace_hdl_t *dtp, char *name, char *path)
{
	uint_t h = dt_strtab_hash(name, NULL) % dtp->dt_kernpathbuckets;
	dt_kern_path_t *dkpp;

	for (dkpp = dtp->dt_kernpaths[h]; dkpp != NULL; dkpp = dkpp->dkp_next) {
		if (strcmp(dkpp->dkp_name, name) == 0) {
			free(name);
			free(path);
			return (dkpp);
		}
	}

	if ((dkpp = malloc(sizeof (dt_kern_path_t))) == NULL)
		return (NULL); /* caller must handle allocation failure */

	dt_dprintf("Adding %s -> %s\n", name, path);

	bzero(dkpp, sizeof (dt_kern_path_t));
	dkpp->dkp_name = name;
	dkpp->dkp_path = path;			/* strdup()ped by our caller */
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
 *
 * Must be called under the kern_path_update_lock.
 */
int
dt_kern_path_update(dtrace_hdl_t *dtp)
{
	assert(MUTEX_HELD(&kern_path_update_lock));

	/*
	 * 10 is a totally arbitrary number way below any likely useful limit on
	 * the fd count.  Anyone setting ulimits below this deserves what they
	 * get.
	 */

	locked_dtp = dtp;
	return nftw(dtp->dt_module_path, dt_kern_path_update_one_entry,
	    10, FTW_PHYS);
}

/*
 * The guts of dt_kern_path_update().
 */
static int
dt_kern_path_update_one_entry(const char *fpath, const struct stat *sb,
    int typeflag, struct FTW *ftwbuf)
{
	char *suffix;
	char *modname;
	char *modpath;

	/*
	 * Fail if whole directories can't be read, since this means we'll miss
	 * modules.
	 */
	switch (typeflag) {
	case FTW_DNR:
		return -EPERM;
	case FTW_F:
		break;
	/*
	 * Unstattable, directory, etc.
	 */
	default:
		return 0;
	}

	suffix = strrstr(&fpath[ftwbuf->base], ".ko");
	if ((suffix == NULL) || (suffix[3] != '\0'))
		return 0;

	modpath = strdup(fpath);
	modname = strndup(&fpath[ftwbuf->base],
	    strlen(&fpath[ftwbuf->base]) - 3);

	if (dt_kern_path_create(locked_dtp, modname, modpath) == NULL) {
		free(modname);
		free(modpath);
		return EDT_NOMEM;
	}

	return 0;
}

dt_kern_path_t *
dt_kern_path_lookup_by_name(dtrace_hdl_t *dtp, const char *name)
{
	uint_t h = dt_strtab_hash(name, NULL) % dtp->dt_kernpathbuckets;
	dt_kern_path_t *dkpp;

	if (dtp->dt_nkernpaths == 0) {
		int dterrno;

		pthread_mutex_lock(&kern_path_update_lock);
		dterrno = dt_kern_path_update(dtp);
		pthread_mutex_unlock(&kern_path_update_lock);

		if (dterrno != 0) {
			dt_dprintf("Error initializing kernel module paths: "
			    "%s\n", dtrace_errmsg(dtp, dterrno));
			return (NULL);
		}

		dt_dprintf("Initialized %i kernel module paths\n",
		    dtp->dt_nkernpaths);
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
dtrace__internal_kern_path_create(dtrace_hdl_t *dtp, char *name, char *path)
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
