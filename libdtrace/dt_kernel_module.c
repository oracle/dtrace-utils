/*
 * Oracle Linux DTrace.
 * Copyright (c) 2009, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * Kernel module list management.  We must maintain bindings from
 * name->filesystem path for all the current kernel's modules, since the system
 * maintains no such list and all mechanisms other than find(1)-analogues have
 * been deprecated or removed in kmod.  However, we can rely on modules.dep
 * to contain paths to all modules, in- or out-of-tree.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <alloca.h>
#include <unistd.h>

#include <dt_kernel_module.h>
#include <dt_string.h>
#include <port.h>

static pthread_mutex_t kern_path_update_lock = PTHREAD_MUTEX_INITIALIZER;

/*
 * On successful return, name and path become owned by dt_kern_path_create()
 * (and may be freed immediately).
 *
 * Note: this code is used by ctf_module_dump as well.
 */

dt_kern_path_t *
dt_kern_path_create(dtrace_hdl_t *dtp, char *name, char *path)
{
	uint_t h = str2hval(name, 0) % dtp->dt_kernpathbuckets;
	dt_kern_path_t *dkpp;

	for (dkpp = dtp->dt_kernpaths[h]; dkpp != NULL; dkpp = dkpp->dkp_next) {
		if (strcmp(dkpp->dkp_name, name) == 0) {
			free(name);
			free(path);
			return dkpp;
		}
	}

	if ((dkpp = malloc(sizeof(dt_kern_path_t))) == NULL)
		return NULL; /* caller must handle allocation failure */

	dt_dprintf("Adding %s -> %s\n", name, path);

	memset(dkpp, 0, sizeof(dt_kern_path_t));
	dkpp->dkp_name = name;
	dkpp->dkp_path = path;			/* strdup()ped by our caller */
	dt_list_append(&dtp->dt_kernpathlist, dkpp);
	dkpp->dkp_next = dtp->dt_kernpaths[h];
	dtp->dt_kernpaths[h] = dkpp;
	dtp->dt_nkernpaths++;

	return dkpp;
}

void
dt_kern_path_destroy(dtrace_hdl_t *dtp, dt_kern_path_t *dkpp)
{
	uint_t h = str2hval(dkpp->dkp_name, 0) % dtp->dt_kernpathbuckets;
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
 * Construct the mapping of kernel module name -> path for all modules,
 * by reading modules.dep.
 *
 * Must be called under the kern_path_update_lock.
 */
int
dt_kern_path_update(dtrace_hdl_t *dtp)
{
	FILE *dep;
	char *buf;
	char *depname;
	char *line = NULL;
	size_t n;
	size_t mplen = strlen(dtp->dt_module_path);
	int err;
	struct stat s;

	assert(MUTEX_HELD(&kern_path_update_lock));

	depname = alloca(strlen(dtp->dt_module_path) + strlen("/modules.dep") + 1);
	strcpy(depname, dtp->dt_module_path);
	strcat(depname, "/modules.dep");

	if (stat(depname, &s) < 0)
		return EDT_OBJIO;

	if ((buf = malloc(s.st_size + 1)) == NULL)
		return EDT_NOMEM;

	if ((dep = fopen(depname, "r")) == NULL) {
		free(buf);
		return EDT_OBJIO;
	}

	setvbuf(dep, buf, _IOFBF, s.st_size + 1);

	errno = 0;
	while (getline(&line, &n, dep) >= 0) {
		char *suffix;
		char *modname;
		char *modpath;
		char *p;

		suffix = strstr(line, ".ko:");
		if (suffix == NULL)
			suffix = strstr(line, ".ko.gz:");

		if (suffix == NULL)
			suffix = strstr(line, ".ko.xz:");

		if (suffix == NULL)
			continue;

		suffix[3] = '\0';		/* chop the dep components */

		modname = strrchr(line, '/');
		if (!modname)
			modname = line;
		else
			modname++;

		modname = strndup(modname, strlen(modname) - 3);
		if (errno == ENOMEM)
			break;

		modpath = malloc(mplen + 1 + strlen(line) + 1);
		if (!modpath) {
			free(modname);
			break; /* OOM */
		}

		p = stpcpy(modpath, dtp->dt_module_path);
		p = stpcpy(p, "/");
		strcpy(p, line);

		if (dt_kern_path_create(dtp, modname, modpath) == NULL)
			break; /* OOM */
	}
	err = errno;

	free(line);
	fclose(dep);
	free(buf);

	if (err == ENOMEM)
		return EDT_NOMEM;
	else
		return err;
}

dt_kern_path_t *
dt_kern_path_lookup_by_name(dtrace_hdl_t *dtp, const char *name)
{
	uint_t h = str2hval(name, 0) % dtp->dt_kernpathbuckets;
	dt_kern_path_t *dkpp;

	if (dtp->dt_nkernpaths == 0) {
		int dterrno;

		pthread_mutex_lock(&kern_path_update_lock);
		dterrno = dt_kern_path_update(dtp);
		pthread_mutex_unlock(&kern_path_update_lock);

		if (dterrno != 0) {
			dt_dprintf("Error initializing kernel module paths: "
			    "%s\n", dtrace_errmsg(dtp, dterrno));
			return NULL;
		}

		dt_dprintf("Initialized %i kernel module paths\n",
		    dtp->dt_nkernpaths);
	}

	for (dkpp = dtp->dt_kernpaths[h]; dkpp != NULL; dkpp = dkpp->dkp_next) {
		if (strcmp(dkpp->dkp_name, name) == 0)
			return dkpp;
	}

	return NULL;
}
