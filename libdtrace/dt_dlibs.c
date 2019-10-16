/*
 * Oracle Linux DTrace.
 * Copyright (c) 2008, 2019, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * TODO:
 * - Sort symbols by address (offset)
 * - FIl in symbol sizes (if needed)
 * - Sort relocations by address (offset)
 * - Associate relocations with symbols
 * - Resolve relocations
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
#include <dt_bpf_builtins.h>

#define DT_DLIB_D	0
#define DT_DLIB_BPF	1

typedef struct dt_bpf_reloc	dt_bpf_reloc_t;
struct dt_bpf_reloc {
	int		sym;
	int		type;
	uint64_t	offset;
	dt_bpf_reloc_t	*next;
};

struct dt_bpf_func {
	const char	*name;
	Elf		*elf;
	int		id;
	int		scn;
	uint64_t	offset;
	size_t		size;
	dt_bpf_reloc_t	*relocs;
	dt_bpf_reloc_t	*last_reloc;
};

static dt_bpf_func_t	*dt_bpf_funcs;
static int		dt_bpf_funcc;
static dt_bpf_reloc_t	*dt_bpf_relocs;
static int		dt_bpf_relocc;

#define DT_BPF_BUILTIN_FN(x, y)	[DT_BPF_##x] = { __stringify(dt_##y), }
dt_bpf_builtin_t	dt_bpf_builtins[] = {
        DT_BPF_MAP_BUILTINS(DT_BPF_BUILTIN_FN)
};
#undef DT_BPF_BUILTIN_FN

/*
 * Compare function (used by qsort) to compare two BPF functions based on their
 * offset in the .text section.  We are sorting in ascending offset order.
 */
static int
symcmp(const void *a, const void *b)
{
	const dt_bpf_func_t	*x = a;
	const dt_bpf_func_t	*y = b;

	return x->offset > y->offset ? 1
				     : x->offset == y->offset ? 0
							      : -1;
}

/*
 * Compare function (used by qsort) to compare two BPF relocations based on
 * their offset in the .text section.  We are sorting in ascending offset
 * order.
 */
static int
relcmp(const void *a, const void *b)
{
	const dt_bpf_reloc_t	*x = a;
	const dt_bpf_reloc_t	*y = b;

	return x->offset > y->offset ? 1
				     : x->offset == y->offset ? 0
							      : -1;
}

/*
 * Populate dt_bpf_funcs with BPF functions from the .text section of the
 * given ELF object.  The dt_bpf_funcs array will contain the functions in
 * the order of their offset into the .text section.
 *
 * TEMPORARY:
 * The gcc BPF compiler generates ELF objects with symbols that have a zero
 * size, so we need to patch that up.
 */
static int
populate_bpf_funcs(Elf *elf, GElf_Ehdr *ehdr)
{
	int		text_idx = -1;
	int		text_siz = 0;
	int		stbl_idx = -1;
	int		strs_idx = -1;
	int		idx, count;
	Elf_Scn		*scn;
	Elf_Data	*data;
	GElf_Shdr	shdr;
	dt_bpf_func_t	*bpff;

	/*
	 * We first collect information about the .text section and the symbol
	 * table.
	 */
	idx = 0;
	scn = NULL;
	while ((scn = elf_nextscn(elf, scn)) != NULL) {
		if (gelf_getshdr(scn, &shdr) == NULL) {
			fprintf(stderr, "Scn %d: : failed to get name\n",
				idx);
			return -1;
		}

		idx++;
		if (shdr.sh_type == SHT_SYMTAB) {
			stbl_idx = idx;
			strs_idx = shdr.sh_link;
                } else if (shdr.sh_type == SHT_PROGBITS) {
			char	*name;

			if (!(shdr.sh_flags & SHF_EXECINSTR))
				continue;
			name = elf_strptr(elf, ehdr->e_shstrndx, shdr.sh_name);
			if (name == NULL) {
				fprintf(stderr, "Scn %d: failed to get name\n",
					idx);
				continue;
			}
			if (strcmp(name, ".text") != 0)
				continue;

			text_idx = idx;
			text_siz = shdr.sh_size;
		}

		if (text_idx > 0 && stbl_idx > 0)
			break;
	}

	scn = elf_getscn(elf, stbl_idx);
	if ((data = elf_getdata(scn, NULL)) == NULL) {
		fprintf(stderr, "Scn %d: Failed to get data\n", stbl_idx);
		return -1;
	}

	/*
	 * Count the nuymber of BPF functions in .text.
	 */
again:
	count = 0;
	for (idx = 0; idx < data->d_size / sizeof(GElf_Sym); idx++) {
		GElf_Sym	sym;

		if (!gelf_getsym(data, idx, &sym))
			continue;
		if (GELF_ST_BIND(sym.st_info) != STB_GLOBAL)
			continue;
		if (sym.st_shndx != text_idx)
			continue;

		/*
		 * Second pass: fill in symbol info
		 */
		if (dt_bpf_funcs) {
			char		*sname;

			sname = elf_strptr(elf, strs_idx, sym.st_name);
			if (sname == NULL)
				fprintf(stderr, "Sym %d: Failed to get name\n",
					idx);

			bpff = &dt_bpf_funcs[count];
			bpff->name = sname ? strdup(sname) : NULL;
			bpff->elf = elf;
			bpff->scn = text_idx;
			bpff->id = idx;
			bpff->offset = sym.st_value;
			bpff->size = sym.st_size;
		}

		count++;
	}

	/*
	 * First pass: allocate dt_bpf_funcs
	 */
	if (!dt_bpf_funcs) {
		dt_bpf_funcs = calloc(count, sizeof(dt_bpf_func_t));
		dt_bpf_funcc = count;
		goto again;
	}

	qsort(dt_bpf_funcs, count, sizeof(dt_bpf_func_t), symcmp);

	/*
	 * Patch up symbols that have a zero size.  This is a workaround for a
	 * bug in the gcc BPF compiler causing symbols to be emitted with a
	 * zero size.
	 */
	idx = count - 1;
	if (dt_bpf_funcs[idx].size == 0)
		dt_bpf_funcs[idx].size = text_siz - dt_bpf_funcs[idx].offset;

	while (--idx >= 0) {
		if (dt_bpf_funcs[idx].size == 0)
			dt_bpf_funcs[idx].size = dt_bpf_funcs[idx + 1].offset -
						 dt_bpf_funcs[idx].offset;
	}

	return 0;
}

static int
collect_bpf_relocs(Elf *elf, GElf_Ehdr *ehdr)
{
	int		text_idx = -1;
	int		relo_idx = -1;
	int		idx, sid;
	uint64_t	soff, eoff;
	Elf_Scn		*scn;
	Elf_Data	*data;
	GElf_Shdr	shdr;
	dt_bpf_reloc_t	*bpfr;

	/*
	 * If there are no BPF functions, we have nothing to do here.
	 */
	if (dt_bpf_funcc == 0)
		return 0;

	/*
	 * Obtain the index of the .text section from the first BPF function
	 * (since all BPF functions we are interested in must reside in .text).
	 */
	text_idx = dt_bpf_funcs[0].scn;

	/*
	 * Look for a relocation section for the .text section.
	 */
	idx = 0;
	scn = NULL;
	while ((scn = elf_nextscn(elf, scn)) != NULL) {
		if (gelf_getshdr(scn, &shdr) == NULL) {
			fprintf(stderr, "Scn %d: : failed to get name\n",
				idx);
			return -1;
		}

		idx++;
		if (shdr.sh_type == SHT_REL) {
			if (shdr.sh_info != text_idx)
				continue;

			relo_idx = idx;
			break;
		}
	}

	if ((data = elf_getdata(scn, NULL)) == NULL) {
		fprintf(stderr, "Scn %d: Failed to get data\n", relo_idx);
		return -1;
	}

	/*
	 * Read the relocation records and store our version of them.  We will
	 * sort them (by ascending offset order) before we process them and
	 * associate them with the functions they belong to.
	 */
	dt_bpf_relocc = data->d_size / sizeof(GElf_Rel);
	dt_bpf_relocs = calloc(dt_bpf_relocc, sizeof(dt_bpf_reloc_t));

	for (idx = 0, bpfr = dt_bpf_relocs; idx < dt_bpf_relocc;
	     idx++, bpfr++) {
		GElf_Rel	rel;

		if (!gelf_getrel(data, idx, &rel))
			continue;

		bpfr->sym = GELF_R_SYM(rel.r_info);
		bpfr->type = GELF_R_TYPE(rel.r_info);
		bpfr->offset =rel.r_offset;
	}

	qsort(dt_bpf_relocs, dt_bpf_relocc, sizeof(dt_bpf_reloc_t), relcmp);

	soff = dt_bpf_funcs[0].offset;
	eoff = dt_bpf_funcc > 1 ? dt_bpf_funcs[1].offset : 0;
	sid = 0;
	for (idx = 0, bpfr = dt_bpf_relocs; idx < dt_bpf_relocc;
	     idx++, bpfr++) {
		if (bpfr->offset < soff)
			continue;

		while (eoff && bpfr->offset >= eoff) {
			sid++;
			soff = eoff;
			eoff = sid < dt_bpf_funcc - 1
					? dt_bpf_funcs[sid + 1].offset
					: 0;
		}

		/*
		 * We adjust the relocation offset to be relative to the start
		 * of the function, since the entire function is going to be
		 * relocated independent of the section.
		 */
		bpfr->offset -= soff;
		if (dt_bpf_funcs[sid].relocs == NULL)
			dt_bpf_funcs[sid].relocs = bpfr;
		else
			dt_bpf_funcs[sid].last_reloc->next = bpfr;

		dt_bpf_funcs[sid].last_reloc = bpfr;
	}

	return 0;
}

static int
readBPFFile(const char *fn)
{
	int		fd;
	int		rc = -1;
	int		i, j;
	Elf		*elf = NULL;
	GElf_Ehdr	ehdr;

	if ((fd = open64(fn, O_RDONLY)) == -1) {
		fprintf(stderr, "Failed to open %s: %s\n",
			fn, strerror(errno));
		return -1;
	}
	if ((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL) {
		fprintf(stderr, "Failed to open ELF in %s\n", fn);
		goto err_elf;
	}
	if (elf_kind(elf) != ELF_K_ELF) {
		fprintf(stderr, "Unsupported ELF file type for %s: %d\n",
			fn, elf_kind(elf));
		goto out;
	}
	if (gelf_getehdr(elf, &ehdr) == NULL) {
		fprintf(stderr, "Failed to read EHDR from %s", fn);
		goto err_elf;
	}
	if (ehdr.e_machine != EM_BPF) {
		fprintf(stderr, "Unsupported ELF machine type %d for %s\n",
			ehdr.e_machine, fn);
		goto out;
	}

	if (populate_bpf_funcs(elf, &ehdr) < 0)
		goto out;
	if (collect_bpf_relocs(elf, &ehdr) < 0)
		goto out;

	for (i = j = 0; i < dt_bpf_funcc && j < DT_BPF_LAST_ID; ) {
		int	d;

		d = strcmp(dt_bpf_funcs[i].name, dt_bpf_builtins[j].name);
fprintf(stderr, "DBG: %d '%s' vs %d '%s' -> d %d\n", i, dt_bpf_funcs[i].name, j, dt_bpf_builtins[j].name, d);
		if (d > 0)
			j++;
		else {
			if (d == 0)
				dt_bpf_builtins[j].sym = &dt_bpf_funcs[i];

			i++;
		}
	}

for (i = 0; i < dt_bpf_funcc; i++) {
  dt_bpf_reloc_t *bpfr;

  fprintf(stderr, "DBG: bpf[%2d] = { '%s', %d, %lu, %lu }\n",
	  i, dt_bpf_funcs[i].name, dt_bpf_funcs[i].id, dt_bpf_funcs[i].offset,
	  dt_bpf_funcs[i].size);

  for (bpfr = dt_bpf_funcs[i].relocs; bpfr; bpfr = bpfr->next)
    fprintf(stderr, "DBG:   Rel   =   { symbol %d, type %s, offset %lu }\n",
	    bpfr->sym, bpfr->type == R_BPF_64_64
				? "INSN_64"
				: bpfr->type == R_BPF_64_32 ? "INSN_DISP32"
							    : "R_BPF_NONE",
	    bpfr->offset);
}

	rc = 0;
	goto out;

err_elf:
	fprintf(stderr, ": %s\n", elf_errmsg(elf_errno()));

out:
	if (elf)
		elf_end(elf);

	close(fd);

	return rc;
}

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

	int type;
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
		if ((p = strrchr(dp->d_name, '.')) == NULL)
			continue; /* skip any filename not containing '.' */
		p++;
		if (strlen(dp->d_name) - (p - dp->d_name) > 1)
			continue; /* suffix is more than 1 character */
		if (*p == 'd')
			type = DT_DLIB_D;
		else if (*p == 'o')
			type = DT_DLIB_BPF;
		else
			continue; /* skip any filename not ending in ".d" */

		(void) snprintf(fname, sizeof (fname),
		    "%s/%s", path, dp->d_name);

		if (type == DT_DLIB_BPF) {
			readBPFFile(fname);
			return 0;
		}

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
