/*
 * Oracle Linux DTrace.
 * Copyright © 2013, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <sys/utsname.h>
#include <errno.h>
#include <libelf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <port.h>
#include <unistd.h>

#include <dt_kernel_module.h>

#define BUCKETS 211

extern int optind;

static const char *default_module_root = "/lib/modules/";
static dtrace_hdl_t dt;
static dtrace_hdl_t *dtp = &dt;


/*
 * Override the dt_dprintf() in libdtrace with one that doesn't pull in half the
 * library, and emits errors as errors.
 */
_dt_printflike_(1,2)
void
dt_dprintf(const char *format, ...)
{
	va_list alist;

	va_start(alist, format);
	vfprintf(stderr, format, alist);
	va_end(alist);
}

static void
usage(int argc, char *argv[])
{
	printf("Syntax: %s [-r kernel-version] [-m module-root] ctf...\n\n", argv[0]);
}

/*
 * Load the section with the given name from the given ELF file and write it out
 * to a new temporary file.
 */
static FILE *
load_sect(const char *elf, const char *name)
{
	const char *s;
	Elf *efp;
	int fd;
	FILE *outfp;
	size_t shstrs;
	GElf_Shdr sh;
	Elf_Data *dp;
	Elf_Scn *sp;

	if ((fd = open(elf, O_RDONLY)) < 0) {
		fprintf(stderr, "Failed to open %s: %s\n",
		    elf, strerror(errno));
		return NULL;
	}

	/*
	 * Use ELF_C_READ_MMAP purely because DTrace does.
	 */
	if ((efp = elf_begin(fd, ELF_C_READ_MMAP, NULL)) == NULL) {
		fprintf(stderr, "Unexpected error from elf_begin(): %s\n",
			elf_errmsg(elf_errno()));
		return NULL;
	}

	if (elf_getshdrstrndx(efp, &shstrs) == -1) {
		fprintf(stderr, "Unexpected error from elf_getshdrstrndx(): "
		    "%s\n", elf_errmsg(elf_errno()));
		exit(1);
	}

	for (sp = NULL; (sp = elf_nextscn(efp, sp)) != NULL; ) {
		if (gelf_getshdr(sp, &sh) == NULL || sh.sh_type == SHT_NULL ||
		    (s = elf_strptr(efp, shstrs, sh.sh_name)) == NULL)
			continue; /* skip any malformed sections */

		if (strcmp(s, name) == 0)
			break; /* section matches specification */
	}

	/*
	 * If the section isn't found, return NULL.
	 */
	if (sp == NULL || (dp = elf_getdata(sp, NULL)) == NULL) {
		fprintf(stderr, "%s: section %s not found.\n", elf, name);
		elf_end(efp);
		return NULL;
	}

	if ((outfp = tmpfile()) == NULL) {
		fprintf(stderr, "Cannot open temporary file: %s\n",
		    strerror(errno));
		exit(1);
	}
	if (fwrite(dp->d_buf, dp->d_size, 1, outfp) < 1) {
		fprintf(stderr, "Cannot write to temporary file: %s\n",
		    strerror(errno));
		exit(1);
	}

	elf_end(efp);
	rewind(outfp);
	return outfp;
}

/*
 * Find and return the compressed CTF content of a module in a new temporary
 * file; if NULL, find the parent module.
 */
static FILE *
find_module_ctf(const char *name)
{
	dt_kern_path_t *dkpp = NULL;
	int shared_module = 0;
	char *secname;
	FILE *fp;

	/*
	 * Non-parent module?  Dig it out.
	 */
	if (name != NULL)
		dkpp = dtrace__internal_kern_path_lookup_by_name(dtp, name);

	if (!dkpp) {
		shared_module = 1;
		dkpp = dtrace__internal_kern_path_lookup_by_name(dtp, "ctf");
	}

	if (!dkpp) {
		fprintf(stderr, "Cannot find kernel module %s.\n",
		    name?name:"ctf");
		return NULL;
	}

	if (!shared_module)
		secname = strdup(".ctf");
	else if (shared_module && name == NULL)
		secname = strdup(".ctf.shared_ctf");
	else {
		secname = malloc(strlen(".ctf.") + strlen(name) + 1);
		strcpy(secname, ".ctf.");
		strcat(secname, name);
	}

	if (secname == NULL) {
		fprintf(stderr, "Out of memory\n");
		exit(1);
	}

	fp = load_sect(dkpp->dkp_path, secname);
	free(secname);
	return fp;
}

/*
 * Dump the CTF in the given module.
 */
static void
ctf_dump(const char *name)
{
	FILE *parent_fp = find_module_ctf(NULL);
	FILE *child_fp = NULL;

	int parent_fd, child_fd;

	if (!parent_fp)
		return;

	parent_fd = fileno(parent_fp);

        printf("\nModule: %s\n", name);
	fflush(stdout);

	if (strcmp(name, "ctf") != 0) {
		child_fp = find_module_ctf(name);

		if (!child_fp) {
			fclose(parent_fp);
			return;
		}

		child_fd = fileno(child_fp);
	}

	/*
	 * Now we have everything open in temporary files, fork off ctf_dump and
	 * point it at those temporary files.
	 */
	switch (fork()) {
	case 0:
	{
		char pfname[PATH_MAX];
		char cfname[PATH_MAX];

		sprintf(pfname, "/proc/self/fd/%i", parent_fd);

		if (!child_fp) {
			execlp("ctf_dump", "ctf_dump", "-q", pfname, NULL);
		} else {
			sprintf(cfname, "/proc/self/fd/%i", child_fd);
			execlp("ctf_dump", "ctf_dump", "-nq", "-p", pfname,
			    cfname, NULL);
		}
		fprintf(stderr, "Cannot execute ctf_dump: %s\n",
		    strerror(errno));
		_exit(1);
	}
	case -1: perror ("fork()");
		break;
	default:
		wait(NULL);
	}

	fclose(parent_fp);

	if (child_fp)
		fclose(child_fp);

	return;
}

int
main(int argc, char *argv[])
{
	char *module_root = NULL;
	char modpath[PATH_MAX];
	char *revno = NULL;
	struct utsname uts;
	char opt;
	dt_kern_path_t *dkpp;

	elf_version(EV_CURRENT);

	if (elf_errno()) {
		fprintf(stderr, "Version synchronization fault: %s\n",
			elf_errmsg(elf_errno()));
		exit(1);
	}

	while ((opt = getopt(argc, argv, "hm:r:")) != -1) {
		switch (opt) {
		case 'h':
			usage(argc, argv);
			exit(1);
		case 'r':
			revno = strdup(optarg);
			break;
		case 'm':
			module_root = strdup(optarg);
			break;
		}
	}

	if (argc == 0 || argc == optind) {
		usage(argc, argv);
		exit(1);
	}

	/*
	 * Without a module root, construct one from the default module root
	 * and the kernel version number or -r argument.
	 */
	if (!module_root) {
		strcpy(modpath, default_module_root);

		if (!revno) {
			uname(&uts);
			strcat(modpath, uts.release);
		} else
			strcat(modpath, revno);
	} else
		strcat(modpath, module_root);

	/*
	 * Construct a skeletal dtrace_hdl, enough to make the module path
	 * searching code happy.
	 */
	dtp->dt_kernpathbuckets = BUCKETS;
	dtp->dt_kernpaths = calloc(dtp->dt_kernpathbuckets, sizeof (dt_kern_path_t *));
	dtp->dt_module_path = strdup(modpath);

	if (dtp->dt_kernpaths == NULL ||
	    dtp->dt_module_path == NULL) {
		fprintf(stderr, "Out of memory\n");
		exit(1);
	}

	while (optind < argc)
		ctf_dump(argv[optind++]);

	while ((dkpp = dt_list_next(&dtp->dt_kernpathlist)) != NULL)
		dtrace__internal_kern_path_destroy(dtp, dkpp);

	free(dtp->dt_kernpaths);
	free(revno);
	free(module_root);

	return 0;
}
