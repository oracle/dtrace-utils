/*
 * Oracle Linux DTrace.
 * Copyright (c) 2008, 2019, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <port.h>
#include <dt_parser.h>
#include <dt_program.h>
#include <dt_grammar.h>
#include <dt_impl.h>

static void
dt_lib_depend_error(dtrace_hdl_t *dtp, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	dt_set_errmsg(dtp, NULL, NULL, NULL, 0, format, ap);
	va_end(ap);
}

int
dt_lib_depend_add(dtrace_hdl_t *dtp, dt_list_t *dlp, const char *arg)
{
	dt_lib_depend_t *dld;
	const char *end;

	assert(arg != NULL);

	if ((end = strrchr(arg, '/')) == NULL)
		return (dt_set_errno(dtp, EINVAL));

	if ((dld = dt_zalloc(dtp, sizeof (dt_lib_depend_t))) == NULL)
		return (-1);

	if ((dld->dtld_libpath = dt_alloc(dtp, PATH_MAX)) == NULL) {
		dt_free(dtp, dld);
		return (-1);
	}

	(void) strlcpy(dld->dtld_libpath, arg, end - arg + 2);
	if ((dld->dtld_library = strdup(arg)) == NULL) {
		dt_free(dtp, dld->dtld_libpath);
		dt_free(dtp, dld);
		return (dt_set_errno(dtp, EDT_NOMEM));
	}

	dt_list_append(dlp, dld);
	return (0);
}

dt_lib_depend_t *
dt_lib_depend_lookup(dt_list_t *dld, const char *arg)
{
	dt_lib_depend_t *dldn;

	for (dldn = dt_list_next(dld); dldn != NULL;
	    dldn = dt_list_next(dldn)) {
		if (strcmp(dldn->dtld_library, arg) == 0)
			return (dldn);
	}

	return (NULL);
}

/*
 * Go through all the library files, and, if any library dependencies exist for
 * that file, add it to that node's list of dependents. The result of this
 * will be a graph which can then be topologically sorted to produce a
 * compilation order.
 */
static int
dt_lib_build_graph(dtrace_hdl_t *dtp)
{
	dt_lib_depend_t *dld, *dpld;

	for (dld = dt_list_next(&dtp->dt_lib_dep); dld != NULL;
	    dld = dt_list_next(dld)) {
		char *library = dld->dtld_library;

		for (dpld = dt_list_next(&dld->dtld_dependencies); dpld != NULL;
		    dpld = dt_list_next(dpld)) {
			dt_lib_depend_t *dlda;

			if ((dlda = dt_lib_depend_lookup(&dtp->dt_lib_dep,
			    dpld->dtld_library)) == NULL) {
				dt_lib_depend_error(dtp,
				    "Invalid library dependency in %s: %s\n",
				    dld->dtld_library, dpld->dtld_library);

				return (dt_set_errno(dtp, EDT_COMPILER));
			}

			if ((dt_lib_depend_add(dtp, &dlda->dtld_dependents,
			    library)) != 0) {
				return (-1); /* preserve dt_errno */
			}
		}
	}
	return (0);
}

static int
dt_topo_sort(dtrace_hdl_t *dtp, dt_lib_depend_t *dld, int *count)
{
	dt_lib_depend_t *dpld, *dlda, *new;

	dld->dtld_start = ++(*count);

	for (dpld = dt_list_next(&dld->dtld_dependents); dpld != NULL;
	    dpld = dt_list_next(dpld)) {
		dlda = dt_lib_depend_lookup(&dtp->dt_lib_dep,
		    dpld->dtld_library);
		assert(dlda != NULL);

		if (dlda->dtld_start == 0 &&
		    dt_topo_sort(dtp, dlda, count) == -1)
			return (-1);
	}

	if ((new = dt_zalloc(dtp, sizeof (dt_lib_depend_t))) == NULL)
		return (-1);

	if ((new->dtld_library = strdup(dld->dtld_library)) == NULL) {
		dt_free(dtp, new);
		return (dt_set_errno(dtp, EDT_NOMEM));
	}

	new->dtld_start = dld->dtld_start;
	new->dtld_finish = dld->dtld_finish = ++(*count);
	dt_list_prepend(&dtp->dt_lib_dep_sorted, new);

	dt_dprintf("library %s sorted (%d/%d)\n", new->dtld_library,
	    new->dtld_start, new->dtld_finish);

	return (0);
}

static int
dt_lib_depend_sort(dtrace_hdl_t *dtp)
{
	dt_lib_depend_t *dld, *dpld, *dlda;
	int count = 0;

	if (dt_lib_build_graph(dtp) == -1)
		return (-1); /* preserve dt_errno */

	/*
	 * Perform a topological sort of the graph that hangs off
	 * dtp->dt_lib_dep. The result of this process will be a
	 * dependency ordered list located at dtp->dt_lib_dep_sorted.
	 */
	for (dld = dt_list_next(&dtp->dt_lib_dep); dld != NULL;
	    dld = dt_list_next(dld)) {
		if (dld->dtld_start == 0 &&
		    dt_topo_sort(dtp, dld, &count) == -1)
			return (-1); /* preserve dt_errno */;
	}

	/*
	 * Check the graph for cycles. If an ancestor's finishing time is
	 * less than any of its dependent's finishing times then a back edge
	 * exists in the graph and this is a cycle.
	 */
	for (dld = dt_list_next(&dtp->dt_lib_dep); dld != NULL;
	    dld = dt_list_next(dld)) {
		for (dpld = dt_list_next(&dld->dtld_dependents); dpld != NULL;
		    dpld = dt_list_next(dpld)) {
			dlda = dt_lib_depend_lookup(&dtp->dt_lib_dep_sorted,
			    dpld->dtld_library);
			assert(dlda != NULL);

			if (dlda->dtld_finish > dld->dtld_finish) {
				dt_lib_depend_error(dtp,
				    "Cyclic dependency detected: %s => %s\n",
				    dld->dtld_library, dpld->dtld_library);

				return (dt_set_errno(dtp, EDT_COMPILER));
			}
		}
	}

	return (0);
}

static void
dt_lib_depend_free(dtrace_hdl_t *dtp)
{
	dt_lib_depend_t *dld, *dlda;

	while ((dld = dt_list_next(&dtp->dt_lib_dep)) != NULL) {
		while ((dlda = dt_list_next(&dld->dtld_dependencies)) != NULL) {
			dt_list_delete(&dld->dtld_dependencies, dlda);
			dt_free(dtp, dlda->dtld_library);
			dt_free(dtp, dlda->dtld_libpath);
			dt_free(dtp, dlda);
		}
		while ((dlda = dt_list_next(&dld->dtld_dependents)) != NULL) {
			dt_list_delete(&dld->dtld_dependents, dlda);
			dt_free(dtp, dlda->dtld_library);
			dt_free(dtp, dlda->dtld_libpath);
			dt_free(dtp, dlda);
		}
		dt_list_delete(&dtp->dt_lib_dep, dld);
		dt_free(dtp, dld->dtld_library);
		dt_free(dtp, dld->dtld_libpath);
		dt_free(dtp, dld);
	}

	while ((dld = dt_list_next(&dtp->dt_lib_dep_sorted)) != NULL) {
		dt_list_delete(&dtp->dt_lib_dep_sorted, dld);
		dt_free(dtp, dld->dtld_library);
		dt_free(dtp, dld);
	}
}

/*
 * Open all of the .d library files found in the specified directory and
 * compile each one in topological order to cache its inlines and translators,
 * etc.  We silently ignore any missing directories and other files found
 * therein. We only fail (and thereby fail dt_load_libs()) if we fail to
 * compile a library and the error is something other than #pragma D depends_on.
 * Dependency errors are silently ignored to permit a library directory to
 * contain libraries which may not be accessible depending on our privileges.
 */
static int
dt_load_libs_dir(dtrace_hdl_t *dtp, const char *path)
{
	struct dirent *dp;
	const char *p;
	DIR *dirp;

	char fname[PATH_MAX];
	dtrace_prog_t *pgp;
	FILE *fp;
	void *rv;
	dt_lib_depend_t *dld;

	if ((dirp = opendir(path)) == NULL) {
		dt_dprintf("skipping lib dir %s: %s\n", path, strerror(errno));
		return (0);
	}

	/* First, parse each file for library dependencies. */
	while ((dp = readdir(dirp)) != NULL) {
		if ((p = strrchr(dp->d_name, '.')) == NULL || strcmp(p, ".d"))
			continue; /* skip any filename not ending in .d */

		(void) snprintf(fname, sizeof (fname),
		    "%s/%s", path, dp->d_name);

		if ((fp = fopen(fname, "r")) == NULL) {
			dt_dprintf("skipping library %s: %s\n",
			    fname, strerror(errno));
			continue;
		}

		dtp->dt_filetag = fname;
		if (dt_lib_depend_add(dtp, &dtp->dt_lib_dep, fname) != 0)
			goto err_close;

		rv = dt_compile(dtp, DT_CTX_DPROG,
		    DTRACE_PROBESPEC_NAME, NULL,
		    DTRACE_C_EMPTY | DTRACE_C_CTL, 0, NULL, fp, NULL);

		if (rv != NULL && dtp->dt_errno &&
		    (dtp->dt_errno != EDT_COMPILER ||
		    dtp->dt_errtag != dt_errtag(D_PRAGMA_DEPEND)))
			goto err_close;

		if (dtp->dt_errno)
			dt_dprintf("error parsing library %s: %s\n",
			    fname, dtrace_errmsg(dtp, dtrace_errno(dtp)));

		(void) fclose(fp);
		dtp->dt_filetag = NULL;
	}

	(void) closedir(dirp);
	/*
	 * Finish building the graph containing the library dependencies
	 * and perform a topological sort to generate an ordered list
	 * for compilation.
	 */
	if (dt_lib_depend_sort(dtp) == -1)
		goto err;

	for (dld = dt_list_next(&dtp->dt_lib_dep_sorted); dld != NULL;
	    dld = dt_list_next(dld)) {

		if ((fp = fopen(dld->dtld_library, "r")) == NULL) {
			dt_dprintf("skipping library %s: %s\n",
			    dld->dtld_library, strerror(errno));
			continue;
		}

		dtp->dt_filetag = dld->dtld_library;
		pgp = dtrace_program_fcompile(dtp, fp, DTRACE_C_EMPTY, 0, NULL);
		(void) fclose(fp);
		dtp->dt_filetag = NULL;

		if (pgp == NULL && (dtp->dt_errno != EDT_COMPILER ||
		    dtp->dt_errtag != dt_errtag(D_PRAGMA_DEPEND)))
			goto err;

		if (pgp == NULL) {
			dt_dprintf("skipping library %s: %s\n",
			    dld->dtld_library,
			    dtrace_errmsg(dtp, dtrace_errno(dtp)));
		} else {
			dld->dtld_loaded = B_TRUE;
			dt_program_destroy(dtp, pgp);
		}
	}

	dt_lib_depend_free(dtp);
	return (0);

err_close:
	(void) fclose(fp);
	(void) closedir(dirp);
err:
	dt_lib_depend_free(dtp);
	return (-1); /* preserve dt_errno */
}

/*
 * Given a library path entry tries to find a per-kernel directory
 * that should be used if present.
 */
static char *
dt_find_kernpath(dtrace_hdl_t *dtp, const char *path)
{
	char		*kern_path = NULL;
	dt_version_t	kver = DT_VERSION_NUMBER(0, 0, 0);
	struct dirent	*dp;
	DIR		*dirp;

	if ((dirp = opendir(path)) == NULL)
		return NULL;

	while ((dp = readdir(dirp)) != NULL) {
		dt_version_t cur_kver;

		/* Skip ., .. and hidden dirs. */
		if (dp->d_name[0] == '.')
			continue;

		/* Try to match kernel version for given file. */
		if (dt_str2kver(dp->d_name, &cur_kver) < 0)
			continue;

		/* Skip newer kernels than ours. */
		if (cur_kver > dtp->dt_kernver)
			continue;

		/* A more recent kernel has been found already. */
		if (cur_kver < kver)
			continue;

		/* Update the iterator state. */
		kver = cur_kver;
		free(kern_path);
		if (asprintf(&kern_path, "%s/%s", path, dp->d_name) < 0) {
			kern_path = NULL;
			break;
		}
	}

	(void) closedir(dirp);
	return kern_path;
}

/*
 * Load the contents of any appropriate DTrace .d library files.  These files
 * contain inlines and translators that will be cached by the compiler.  We
 * defer this activity until the first compile to permit libdtrace clients to
 * add their own library directories and so that we can properly report errors.
 */
int
dt_load_libs(dtrace_hdl_t *dtp)
{
	dt_dirpath_t *dirp;

	if (dtp->dt_cflags & DTRACE_C_NOLIBS)
		return (0); /* libraries already processed */

	dtp->dt_cflags |= DTRACE_C_NOLIBS;

	for (dirp = dt_list_next(&dtp->dt_lib_path);
	    dirp != NULL; dirp = dt_list_next(dirp)) {
		char *kdir_path;

		/* Load libs from per-kernel path if available. */
		if ((kdir_path = dt_find_kernpath(dtp, dirp->dir_path)) != NULL) {
			if (dt_load_libs_dir(dtp, kdir_path) != 0) {
				dtp->dt_cflags &= ~DTRACE_C_NOLIBS;
				free(kdir_path);
				return (-1);
			}
			free(kdir_path);
		}

		/* Load libs from original path in the list. */
		if (dt_load_libs_dir(dtp, dirp->dir_path) != 0) {
			dtp->dt_cflags &= ~DTRACE_C_NOLIBS;
			return (-1); /* errno is set for us */
		}
	}

	return (0);
}
