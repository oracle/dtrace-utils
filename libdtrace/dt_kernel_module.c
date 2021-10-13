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

static uint32_t
kernpath_hval(const dt_kern_path_t *dkp)
{
	return str2hval(dkp->dkp_name, 0);
}

static int
kernpath_cmp(const dt_kern_path_t *p,
	     const dt_kern_path_t *q)
{
	return strcmp(p->dkp_name, q->dkp_name);
}

DEFINE_HE_STD_LINK_FUNCS(kernpath, dt_kern_path_t, dkp_he)

static void *
kernpath_del_path(dt_kern_path_t *head, dt_kern_path_t *dkpp)
{
	head = kernpath_del(head, dkpp);
	free(dkpp->dkp_name);
	free(dkpp->dkp_path);
	free(dkpp);
	return head;
}

static dt_htab_ops_t kernpath_htab_ops = {
	.hval = (htab_hval_fn)kernpath_hval,
	.cmp = (htab_cmp_fn)kernpath_cmp,
	.add = (htab_add_fn)kernpath_add,
	.del = (htab_del_fn)kernpath_del_path,
	.next = (htab_next_fn)kernpath_next
};

/*
 * On successful return, name and path become owned by dt_kern_path_create()
 * (and may be freed immediately).  The returned entry is owned by the
 * dt_kernpaths hash into which it is interned.
 */

dt_kern_path_t *
dt_kern_path_create(dtrace_hdl_t *dtp, char *name, char *path)
{
	dt_kern_path_t *dkpp;
	dt_kern_path_t tmpl;

	if (!dtp->dt_kernpaths) {
		dtp->dt_kernpaths = dt_htab_create(dtp, &kernpath_htab_ops);

		if (!dtp->dt_kernpaths)
			return NULL; /* caller must handle allocation failure */
	}

	tmpl.dkp_name = name;
	if ((dkpp = dt_htab_lookup(dtp->dt_kernpaths, &tmpl)) != NULL) {
		free(name);
		free(path);
		return dkpp;
	}

	if ((dkpp = malloc(sizeof(dt_kern_path_t))) == NULL)
		return NULL;

	dt_dprintf("Adding %s -> %s\n", name, path);

	memset(dkpp, 0, sizeof(dt_kern_path_t));
	dkpp->dkp_name = name;
	dkpp->dkp_path = path;			/* strdup()ped by our caller */
	if (dt_htab_insert(dtp->dt_kernpaths, dkpp) < 0) {
		free(dkpp);
		return NULL;
	}

	return dkpp;
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
	dt_kern_path_t tmpl;

	if (!dtp->dt_kernpaths) {
		int dterrno;

		pthread_mutex_lock(&kern_path_update_lock);
		dterrno = dt_kern_path_update(dtp);
		pthread_mutex_unlock(&kern_path_update_lock);

		if (dterrno != 0) {
			dt_dprintf("Error initializing kernel module paths: "
			    "%s\n", dtrace_errmsg(dtp, dterrno));
			return NULL;
		}

		dt_dprintf("Initialized %zi kernel module paths\n",
		    dt_htab_entries(dtp->dt_kernpaths));
	}

	tmpl.dkp_name = (char *) name;
	return dt_htab_lookup(dtp->dt_kernpaths, &tmpl);
}
