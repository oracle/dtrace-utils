/*
 * Oracle Linux DTrace.
 * Copyright (c) 2008, 2021, Oracle and/or its affiliates. All rights reserved.
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
#include <dt_bpf.h>

#define DT_DLIB_D	0
#define DT_DLIB_BPF	1

typedef struct dt_bpf_reloc	dt_bpf_reloc_t;
struct dt_bpf_reloc {
	int		sym;
	int		type;
	uint64_t	offset;
	dt_bpf_reloc_t	*next;
};

typedef struct dt_bpf_func	dt_bpf_func_t;
struct dt_bpf_func {
	char		*name;
	int		id;
	uint64_t	offset;
	size_t		size;
	dt_ident_t	*ident;
	dtrace_difo_t	*difo;
	dt_bpf_reloc_t	*relocs;
	dt_bpf_reloc_t	*last_reloc;
};

static dtrace_attribute_t	dt_bpf_attr = DT_ATTR_STABCMN;

#define DT_BPF_SYMBOL(name, type) \
	{ __stringify(name), (type), DT_IDFLG_BPF, DT_IDENT_UNDEF, \
		DT_ATTR_STABCMN, DT_VERS_2_0, NULL, NULL, NULL, }
#define DT_BPF_SYMBOL_ID(name, type, id) \
	{ __stringify(name), (type), DT_IDFLG_BPF, (id), \
		DT_ATTR_STABCMN, DT_VERS_2_0, NULL, NULL, NULL, }

static const dt_ident_t		dt_bpf_symbols[] = {
	/* BPF built-in functions */
	DT_BPF_SYMBOL(dt_program, DT_IDENT_FUNC),
	/* BPF library (external) functions */
	DT_BPF_SYMBOL(dt_agg_lqbin, DT_IDENT_SYMBOL),
	DT_BPF_SYMBOL(dt_agg_qbin, DT_IDENT_SYMBOL),
	DT_BPF_SYMBOL(dt_error, DT_IDENT_SYMBOL),
	DT_BPF_SYMBOL(dt_get_bvar, DT_IDENT_SYMBOL),
	DT_BPF_SYMBOL(dt_get_string, DT_IDENT_SYMBOL),
	DT_BPF_SYMBOL(dt_get_tvar, DT_IDENT_SYMBOL),
	DT_BPF_SYMBOL(dt_set_tvar, DT_IDENT_SYMBOL),
	DT_BPF_SYMBOL(dt_strjoin, DT_IDENT_SYMBOL),
	DT_BPF_SYMBOL(dt_strnlen, DT_IDENT_SYMBOL),
	/* BPF maps */
	DT_BPF_SYMBOL(aggs, DT_IDENT_PTR),
	DT_BPF_SYMBOL(buffers, DT_IDENT_PTR),
	DT_BPF_SYMBOL(cpuinfo, DT_IDENT_PTR),
	DT_BPF_SYMBOL(gvars, DT_IDENT_PTR),
	DT_BPF_SYMBOL(lvars, DT_IDENT_PTR),
	DT_BPF_SYMBOL(mem, DT_IDENT_PTR),
	DT_BPF_SYMBOL(probes, DT_IDENT_PTR),
	DT_BPF_SYMBOL(state, DT_IDENT_PTR),
	DT_BPF_SYMBOL(strtab, DT_IDENT_PTR),
	DT_BPF_SYMBOL(tvars, DT_IDENT_PTR),
	/* BPF internal identifiers */
	DT_BPF_SYMBOL_ID(EPID, DT_IDENT_SCALAR, DT_CONST_EPID),
	DT_BPF_SYMBOL_ID(PRID, DT_IDENT_SCALAR, DT_CONST_PRID),
	DT_BPF_SYMBOL_ID(CLID, DT_IDENT_SCALAR, DT_CONST_CLID),
	DT_BPF_SYMBOL_ID(ARGC, DT_IDENT_SCALAR, DT_CONST_ARGC),
	DT_BPF_SYMBOL_ID(STBSZ, DT_IDENT_SCALAR, DT_CONST_STBSZ),
	DT_BPF_SYMBOL_ID(STRSZ, DT_IDENT_SCALAR, DT_CONST_STRSZ),
	DT_BPF_SYMBOL_ID(STKSIZ, DT_IDENT_SCALAR, DT_CONST_STKSIZ),
	DT_BPF_SYMBOL_ID(BOOTTM, DT_IDENT_SCALAR, DT_CONST_BOOTTM),
	/* End-of-list marker */
	{ NULL, }
};
#undef DT_BPF_SYMBOL
#undef DT_BPF_SYMBOL_ID

static void
dt_dlib_error(dtrace_hdl_t *dtp, int eid, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	dt_set_errmsg(dtp, dt_errtag(eid), NULL, NULL, 0, format, ap);
	va_end(ap);
}

/*
 * Perform initialization for the BPF function library handling.
 */
void
dt_dlib_init(dtrace_hdl_t *dtp)
{
	dtp->dt_bpfsyms = dt_idhash_create("BPF symbols", dt_bpf_symbols,
					   1000, UINT_MAX);
}

/*
 * Lookup a BPF identifier by name.
 */
dt_ident_t *
dt_dlib_get_sym(dtrace_hdl_t *dtp, const char *name)
{
	dt_ident_t	*idp = dt_idhash_lookup(dtp->dt_bpfsyms, name);

	if (idp == NULL)
		return NULL;
	if ((idp->di_flags & DT_IDFLG_BPF) == 0)
		return NULL;

	return idp;
}

/*
 * Lookup a BPF identifier of a specific kind by name.
 */
static dt_ident_t *
dt_dlib_get_xsym(dtrace_hdl_t *dtp, const char *name, int kind)
{
	dt_ident_t	*idp = dt_dlib_get_sym(dtp, name);

	if (idp == NULL || idp->di_kind != kind)
		return NULL;

	return idp;
}

/*
 * Add a BPF identifier of a given kind with given id.
 */
static dt_ident_t *
dt_dlib_add_sym_id(dtrace_hdl_t *dtp, const char *name, int kind, uint_t id)
{
	dt_idhash_t	*dhp = dtp->dt_bpfsyms;
	dt_ident_t	*idp;

	idp = dt_idhash_insert(dhp, name, kind, DT_IDFLG_BPF, id,
			       dt_bpf_attr, DT_VERS_2_0,
			       kind == DT_IDENT_SYMBOL ? &dt_idops_difo : NULL,
			       dtp, 0);

	return idp;
}

/*
 * Add a BPF identifier of a given kind.
 */
static dt_ident_t *
dt_dlib_add_sym(dtrace_hdl_t *dtp, const char *name, int kind)
{
	return dt_dlib_add_sym_id(dtp, name, kind, DT_IDENT_UNDEF);
}

/*
 * Lookup a BPF function identifier by name.  We first match against built-in
 * functions, and if not found there, we try external symbols.
 */
dt_ident_t *
dt_dlib_get_func(dtrace_hdl_t *dtp, const char *name)
{
	dt_ident_t	*idp = dt_dlib_get_xsym(dtp, name, DT_IDENT_FUNC);

	if (idp)
		return idp;

	return dt_dlib_get_xsym(dtp, name, DT_IDENT_SYMBOL);
}

/*
 * Add a BPF function.
 */
dt_ident_t *
dt_dlib_add_func(dtrace_hdl_t *dtp, const char *name)
{
	return dt_dlib_add_sym(dtp, name, DT_IDENT_SYMBOL);
}

/*
 * Lookup a BPF map identifier by name.
 */
dt_ident_t *
dt_dlib_get_map(dtrace_hdl_t *dtp, const char *name)
{
	return dt_dlib_get_xsym(dtp, name, DT_IDENT_PTR);
}

/*
 * Add a BPF map.
 */
dt_ident_t *
dt_dlib_add_map(dtrace_hdl_t *dtp, const char *name)
{
	return dt_dlib_add_sym(dtp, name, DT_IDENT_PTR);
}

/*
 * Lookup a BPF variable identifier by name.
 */
dt_ident_t *
dt_dlib_get_var(dtrace_hdl_t *dtp, const char *name)
{
	return dt_dlib_get_xsym(dtp, name, DT_IDENT_SCALAR);
}

/*
 * Add a BPF variable.
 */
dt_ident_t *
dt_dlib_add_var(dtrace_hdl_t *dtp, const char *name, uint_t id)
{
	return dt_dlib_add_sym_id(dtp, name, DT_IDENT_SCALAR, id);
}

/*
 * Return the DIFO for an external symbol.
 */
dtrace_difo_t *
dt_dlib_get_func_difo(dtrace_hdl_t *dtp, const dt_ident_t *idp)
{
	assert(idp->di_kind == DT_IDENT_SYMBOL);

	if (idp->di_data == NULL) {
		dt_dlib_error(dtp, D_IDENT_UNDEF, "dlib symbol %s not found",
			      idp->di_name);
		dt_set_errno(dtp, EDT_COMPILER);
		return NULL;
	}

	return idp->di_data;
}

/*
 * Reset the reference bit on BPF external symbols.
 */
static int
dt_dlib_idreset(dt_idhash_t *dhp, dt_ident_t *idp, boolean_t *ref_only)
{
	if (idp->di_kind == DT_IDENT_SYMBOL) {
		idp->di_flags &= ~DT_IDFLG_REF;

		if (!*ref_only)
			idp->di_id = DT_IDENT_UNDEF;
	}
	return 0;
}

/*
 * Reset all BPF external symbols.  This removes the reference bit from the
 * identifier flags to indicate that the symbols are not referenced by any
 * code anymore.
 */
void
dt_dlib_reset(dtrace_hdl_t *dtp, boolean_t ref_only)
{
	dt_idhash_iter(dtp->dt_bpfsyms, (dt_idhash_f *)dt_dlib_idreset,
		       &ref_only);
}

/*
 * Compare function (used by qsort) to compare two BPF functions based on their
 * offset in the .text section.  We are sorting in ascending offset order.
 *
 * Note that we are sorting an array of pointers to BPF functions (incl. fake
 * functions representing BPF maps), and some of the pointers may be NULL.  We
 * ensure that NULL pointers and pointers to BPF maps are moved to the front of
 * the array.
 */
static int
symcmp(const void *a, const void *b)
{
	const dt_bpf_func_t	*x = *(dt_bpf_func_t **)a;
	const dt_bpf_func_t	*y = *(dt_bpf_func_t **)b;

	if (x == NULL || x->id == -1)
		return y == NULL  || y->id == -1? 0 : -1;
	else if (y == NULL || y->id == -1)
		return 1;


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
 * Process the symbols in the ELF object, resolving BPF maps and functions
 * against the known identifiers  If an unknown identifier is encountered, we
 * add it to the BPF identifier hash.
 *
 * For functions, we fix up the symbol size (to work around an issue with the
 * gcc BPF compiler not emitting symbol sizes), and then associate any listed
 * relocations with each function.
 */
static int
get_symbols(dtrace_hdl_t *dtp, Elf *elf, int syms_idx, int strs_idx,
	    int text_idx, int text_len, int relo_idx, int maps_idx)
{
	int		idx, fid, fun0;
	Elf_Scn		*scn;
	Elf_Data	*symtab, *data, *text;
	dt_ident_t	*idp;
	dt_bpf_func_t	**syms = NULL;
	dt_bpf_func_t	**funcs = NULL;
	dt_bpf_func_t	*fp;
	int		symc = 0;
	dt_bpf_reloc_t	*rels = NULL;
	dt_bpf_reloc_t	*rp;
	int		relc;
	uint64_t	soff, eoff;
	dt_strtab_t	*stab;

	/*
	 * Retrieve the actual symbol table from the ELF object.
	 */
	scn = elf_getscn(elf, syms_idx);
	if ((symtab = elf_getdata(scn, NULL)) == NULL) {
		dt_dlib_error(dtp, 0, "BPF ELF: no .symtab data");
		goto err;
	}

	/*
	 * Allocate the symbol list.
	 */
	symc = symtab->d_size / sizeof(GElf_Sym);
	syms = dt_calloc(dtp, symc, sizeof(dt_bpf_func_t *));
	if (syms == NULL)
		goto err;

	/*
	 * Process the symbol table.
	 */
	for (idx = 0; idx < symc; idx++) {
		GElf_Sym	sym;
		char		*name;

		if (!gelf_getsym(symtab, idx, &sym))
			continue;
		if (GELF_ST_BIND(sym.st_info) != STB_GLOBAL)
			continue;

		name = elf_strptr(elf, strs_idx, sym.st_name);
		if (name == NULL) {
			dt_dlib_error(dtp, 0, "BPF ELF: no symbol name");
			goto err;
		}

		if (sym.st_shndx == text_idx) {
			/*
			 * BPF function.
			 */
			idp = dt_dlib_get_func(dtp, name);
			if (idp == NULL) {
				idp = dt_dlib_add_func(dtp, name);
				if (idp == NULL) {
					dt_dprintf("BPF ELF: cannot add %s\n",
						   name);
					continue;
				}
			}

			fp = dt_alloc(dtp, sizeof(dt_bpf_func_t));
			if (fp == NULL)
				goto err;

			fp->name = name ? strdup(name) : NULL;
			fp->id = sym.st_shndx == SHN_UNDEF ? -1 : idx;
			fp->offset = sym.st_value;
			fp->size = sym.st_size;
			fp->ident = idp;
			fp->difo = NULL;
			fp->relocs = NULL;
			fp->last_reloc = NULL;
			syms[idx] = fp;
		} else if (sym.st_shndx == maps_idx || !sym.st_shndx) {
			/*
			 * BPF maps are either declared in the 'maps' section
			 * or they are undefined symbols (no section).  Either
			 * way, DTrace must already know the map by name.
			 *
			 * FUTURE: We may add support for creating maps based
			 *         on a map descriptor in the BPF library.
			 *
			 * We put BPF maps in the symbol list as if they are a
			 * function (yes, really).  They are marked with
			 * id == -1, * offset 0, and size 0.
			 *
			 * We do this because we need these symbols when we
			 * process relocations.  Once we are done with that, we
			 * will get rid of these fake function symbols.
			 */
			idp = dt_dlib_get_map(dtp, name);
			if (idp == NULL)
				idp = dt_dlib_get_func(dtp, name);
			if (idp == NULL)
				idp = dt_dlib_get_var(dtp, name);
			if (idp == NULL) {
				dt_dlib_error(dtp, D_IDENT_UNDEF,
					      "undefined symbol %s in BPF dlib",
					      name);
				goto err;
			}

			fp = dt_alloc(dtp, sizeof(dt_bpf_func_t));
			if (fp == NULL)
				goto err;

			fp->name = name ? strdup(name) : NULL;
			fp->id = -1;
			fp->offset = 0;
			fp->size = 0;
			fp->ident = idp;
			fp->difo = NULL;
			fp->relocs = NULL;
			fp->last_reloc = NULL;
			syms[idx] = fp;
		}
	}

	/*
	 * Make a copy of the symbol list, and sort it based on symbol offset.
	 * The sorting also ensures that NULL-elements in the symbol list are
	 * moved to the very front of the array.
	 *
	 * Symbol lists are merely lists of pointers to the actual data items,
	 * so modifications made through the (sorted) copy will still affect
	 * the actual data items.
	 */
	funcs = dt_calloc(dtp, symc, sizeof(dt_bpf_func_t *));
	if (funcs == NULL)
		goto err;

	memcpy(funcs, syms, symc * sizeof(dt_bpf_func_t *));
	qsort(funcs, symc, sizeof(dt_bpf_func_t *), symcmp);

	/*
	 * Patch up symbols that have a zero size.  This is a workaround for a
	 * bug in the gcc BPF compiler causing symbols to be emitted with a
	 * zero size.
	 */
	idx = symc - 1;
	if (funcs[idx] == NULL)
		goto out;
	if (funcs[idx]->size == 0)
		funcs[idx]->size = text_len - funcs[idx]->offset;

	while (--idx >= 0) {
		if (funcs[idx] == NULL || funcs[idx]->id == -1)
			break;
		if (funcs[idx]->size == 0)
			funcs[idx]->size = funcs[idx + 1]->offset -
						 funcs[idx]->offset;
	}

	/*
	 * Set 'fun0' to be the index of the first function in the funcs list
	 * (i.e. the function with the lowest offset, usually 0).
	 *
	 * Note that the assignment is always valid because if funcs[0] is not
	 * NULL (and therefore the first function), the loop above will end
	 * with idx == -1.
	 */
	fun0 = idx + 1;

	/*
	 * Retrieve the executable code (.text) from the ELF object.
	 */
	scn = elf_getscn(elf, text_idx);
	if ((text = elf_getdata(scn, NULL)) == NULL) {
		dt_dlib_error(dtp, 0, "BPF ELF: no .text data\n");
		goto err;
	}

	for (idx = 0; idx < symc; idx++) {
		dtrace_difo_t	*dp;

		fp = syms[idx];
		if (fp == NULL || fp->id == -1)
			continue;

		dp = dt_zalloc(dtp, sizeof(dtrace_difo_t));
		if (dp == NULL)
			goto err;

		/*
		 * The gcc BPF compiler may add 8 bytes of 0-padding to BPF
		 * functions that do not end on a 128-bit boundary despite the
		 * fact that BPF is a 64-bit architecture.  We need to remove
		 * that padding.
		 */
		if (fp->size % sizeof(struct bpf_insn)) {
			dt_free(dtp, dp);
			dt_dlib_error(dtp, 0, "BPF ELF: %s is truncated",
				      fp->name);
			goto err;
		}
		if (*(uint64_t *)((char*)text->d_buf + fp->offset + fp->size -
				  sizeof(struct bpf_insn)) == 0)
			fp->size -= sizeof(struct bpf_insn);

		dp->dtdo_buf = dt_alloc(dtp, fp->size);
		dp->dtdo_len = fp->size / sizeof(struct bpf_insn);
		if (dp->dtdo_buf == NULL)
			goto err;

		memcpy(dp->dtdo_buf, (char *)text->d_buf + fp->offset,
		       fp->size);
		fp->difo = dp;
	}

	/*
	 * Retrieve the relocation data from the ELF object.
	 */
	scn = elf_getscn(elf, relo_idx);
	if ((data = elf_getdata(scn, NULL)) == NULL) {
		dt_dlib_error(dtp, 0, "BPF ELF: no relocation data");
		goto err;
	}

	/*
	 * Read the relocation records and store our version of them.  We will
	 * sort them (by ascending offset order) before we process them and
	 * associate them with the functions they belong to.  We also compute
	 * the number of relocations for a given function and store it in the
	 * DIFO.  This will help us when we construct the actual DIFO relocs.
	 */
	relc = data->d_size / sizeof(GElf_Rel);
	rels = dt_calloc(dtp, relc, sizeof(dt_bpf_reloc_t));
	if (rels == NULL)
		goto err;

	for (idx = 0, rp = rels; idx < relc; idx++, rp++) {
		GElf_Rel	rel;

		if (!gelf_getrel(data, idx, &rel))
			continue;

		rp->sym = GELF_R_SYM(rel.r_info);
		rp->type = GELF_R_TYPE(rel.r_info);
		rp->offset = rel.r_offset;
	}

	qsort(rels, relc, sizeof(dt_bpf_reloc_t), relcmp);

	/*
	 * Initialize 'fid' and 'fp' to be the first function index and a
	 * pointer to the first function respectively in the funcs list.
	 *
	 * We set soff and eoff such that the address range for 'fp' is
	 * [soff .. eoff[.
	 */
	fid = fun0;
	fp = funcs[fid];
	soff = fp->offset;
	eoff = soff + fp->size;
	for (idx = 0, rp = rels; idx < relc; idx++, rp++) {
		if (rp->offset < soff)
			continue;

		while (rp->offset >= eoff) {
			fid++;
			if (fid >= symc)
				goto done;

			fp = funcs[fid];
			soff = fp->offset;
			eoff = soff + fp->size;
		}

		/*
		 * We adjust the relocation offset to be relative to the start
		 * of the function, since the entire function is going to be
		 * relocated independent of the section.
		 */
		rp->offset -= soff;
		if (fp->relocs == NULL)
			fp->relocs = rp;
		else
			fp->last_reloc->next = rp;

		fp->last_reloc = rp;
		fp->difo->dtdo_brelen++;
	}

done:
	/*
	 * We now post-process the function symbols in order to convert their
	 * relocations into DIFO format.  We again start from the first
	 * function index.
	 *
	 * As we complete the DIFO for a function symbol we modify the function
	 * identifier to store the DIFO as its data (and we ensure that our
	 * idops get installed).
	 */
	for (fid = fun0; fid < symc; fid++) {
		dtrace_difo_t	*dp;
		dof_relodesc_t	*brp;

		fp = funcs[fid];
		dp = fp->difo;
		relc = dp->dtdo_brelen;
		if (relc == 0)
			goto setdata;

		stab = dt_strtab_create(BUFSIZ);
		dp->dtdo_breltab = dt_calloc(dtp, relc, sizeof(dof_relodesc_t));
		if (dp->dtdo_breltab == NULL)
			goto err;

		rp = fp->relocs;
		brp = dp->dtdo_breltab;
		while (relc--) {
			ssize_t	soff = dt_strtab_insert(stab,
							syms[rp->sym]->name);

			brp->dofr_type = rp->type;
			brp->dofr_offset = rp->offset;
			brp->dofr_name = soff;
			brp->dofr_data = 0;
			brp++;
			rp++;
		}

		dp->dtdo_strlen = dt_strtab_size(stab);
		if (dp->dtdo_strlen > 0) {
			dp->dtdo_strtab = dt_alloc(dtp, dp->dtdo_strlen);
			if (dp->dtdo_strtab == NULL)
				goto err;

			dt_strtab_write(stab,
					(dt_strtab_write_f *)dt_strtab_copystr,
					dp->dtdo_strtab);
		} else
			dp->dtdo_strtab = NULL;

		dt_strtab_destroy(stab);

setdata:
		idp = fp->ident;
		dt_ident_morph(idp, idp->di_kind, &dt_idops_difo, dtp);
		dt_ident_set_data(idp, fp->difo);
		fp->difo = NULL;
	}

out:
	/*
	 * Clean up temporary function symbols.
	 */
	for (idx = 0; idx < symc; idx++) {
		fp = syms[idx];
		if (fp == NULL)
			continue;

		if (fp->name)
			free(fp->name);
		if (fp->difo)
			dt_difo_free(dtp, fp->difo);

		dt_free(dtp, fp);
	}

	dt_free(dtp, syms);
	dt_free(dtp, rels);
	dt_free(dtp, funcs);

	return symc == 0 ? -1 : 0;

err:
	symc = 0;

	goto out;
}

static int
readBPFFile(dtrace_hdl_t *dtp, const char *fn)
{
	int		fd;
	int		rc = -1;
	int		text_idx = -1;
	int		text_len = 0;
	int		relo_idx = -1;
	int		maps_idx = -1;
	int		syms_idx = -1;
	int		strs_idx = -1;
	int		idx;
	Elf		*elf = NULL;
	Elf_Scn		*scn;
	GElf_Ehdr	ehdr;
	GElf_Shdr	shdr;

	/*
	 * Open the ELF object file and perform basic validation.
	 */
	if ((fd = open64(fn, O_RDONLY)) == -1) {
		dt_dlib_error(dtp, 0, "%s: %s\n", fn, strerror(errno));
		return -1;
	}
	if ((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL)
		goto err_elf;
	if (elf_kind(elf) != ELF_K_ELF) {
		dt_dlib_error(dtp, 0, "%s: ELF file type not supported", fn);
		goto out;
	}
	if (gelf_getehdr(elf, &ehdr) == NULL)
		goto err_elf;
	if (ehdr.e_machine != EM_BPF) {
		dt_dlib_error(dtp, 0, "%s: ELF machine type not supported", fn);
		goto out;
	}

	/*
	 * Collect information about sections that we ae interested in:
	 *   .text	-- executable code
	 *   .rel.text	-- relocations for the executable code
	 *   .maps	-- BPF maps
	 *   .symtab	-- symbol table
	 *   .strtab	-- string table
	 */
	idx = 0;
	scn = NULL;
	while ((scn = elf_nextscn(elf, scn)) != NULL) {
		idx++;

		if (gelf_getshdr(scn, &shdr) == NULL)
			goto err_elf;

		switch (shdr.sh_type) {
		case SHT_SYMTAB:
			/*
			 * The symbol table contains a reference to the string
			 * table, so we can just pick it up from there.
			 */
			syms_idx = idx;
			strs_idx = shdr.sh_link;
			break;
		case SHT_PROGBITS: {
			char	*name;

			name = elf_strptr(elf, ehdr.e_shstrndx, shdr.sh_name);
			if (name == NULL)
				goto err_elf;

			if (strcmp(name, ".text") == 0) {
				text_idx = idx;
				text_len = shdr.sh_size;
			} else if (strcmp(name, "maps") == 0) {
				maps_idx = idx;
			} else
				continue;

			break;
		}
		case SHT_REL:
			if (shdr.sh_info != text_idx)
				continue;

			relo_idx = idx;
			break;
		}

		/*
		 * If we have found all the sections we were looking for, we
		 * can be done.
		 */
		if (text_idx > 0 && relo_idx > 0 && maps_idx > 0 &&
		    syms_idx > 0 && strs_idx > 0)
			break;
	}

	rc = get_symbols(dtp, elf, syms_idx, strs_idx, text_idx, text_len,
			 relo_idx, maps_idx);
	goto out;

err_elf:
	dt_dlib_error(dtp, 0, "BPF ELF: %s", elf_errmsg(elf_errno()));

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
		return dt_set_errno(dtp, EINVAL);

	if ((dld = dt_zalloc(dtp, sizeof(dt_lib_depend_t))) == NULL)
		return -1;

	if ((dld->dtld_libpath = dt_alloc(dtp, PATH_MAX)) == NULL) {
		dt_free(dtp, dld);
		return -1;
	}

	strlcpy(dld->dtld_libpath, arg, end - arg + 2);
	if ((dld->dtld_library = strdup(arg)) == NULL) {
		dt_free(dtp, dld->dtld_libpath);
		dt_free(dtp, dld);
		return dt_set_errno(dtp, EDT_NOMEM);
	}

	dt_list_append(dlp, dld);
	return 0;
}

dt_lib_depend_t *
dt_lib_depend_lookup(dt_list_t *dld, const char *arg)
{
	dt_lib_depend_t *dldn;

	for (dldn = dt_list_next(dld); dldn != NULL;
	     dldn = dt_list_next(dldn)) {
		if (strcmp(dldn->dtld_library, arg) == 0)
			return dldn;
	}

	return NULL;
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

				return dt_set_errno(dtp, EDT_COMPILER);
			}

			if ((dt_lib_depend_add(dtp, &dlda->dtld_dependents,
					        library)) != 0)
				return -1; /* preserve dt_errno */
		}
	}
	return 0;
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
			return -1;
	}

	if ((new = dt_zalloc(dtp, sizeof(dt_lib_depend_t))) == NULL)
		return -1;

	if ((new->dtld_library = strdup(dld->dtld_library)) == NULL) {
		dt_free(dtp, new);
		return dt_set_errno(dtp, EDT_NOMEM);
	}

	new->dtld_start = dld->dtld_start;
	new->dtld_finish = dld->dtld_finish = ++(*count);
	dt_list_prepend(&dtp->dt_lib_dep_sorted, new);

	dt_dprintf("library %s sorted (%d/%d)\n", new->dtld_library,
		   new->dtld_start, new->dtld_finish);

	return 0;
}

static int
dt_lib_depend_sort(dtrace_hdl_t *dtp)
{
	dt_lib_depend_t *dld, *dpld, *dlda;
	int count = 0;

	if (dt_lib_build_graph(dtp) == -1)
		return -1; /* preserve dt_errno */

	/*
	 * Perform a topological sort of the graph that hangs off
	 * dtp->dt_lib_dep. The result of this process will be a
	 * dependency ordered list located at dtp->dt_lib_dep_sorted.
	 */
	for (dld = dt_list_next(&dtp->dt_lib_dep); dld != NULL;
	     dld = dt_list_next(dld)) {
		if (dld->dtld_start == 0 &&
		    dt_topo_sort(dtp, dld, &count) == -1)
			return -1; /* preserve dt_errno */;
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

				return dt_set_errno(dtp, EDT_COMPILER);
			}
		}
	}

	return 0;
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
		return 0;
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

		snprintf(fname, sizeof(fname), "%s/%s", path, dp->d_name);

		if (type == DT_DLIB_BPF) {
			if (readBPFFile(dtp, fname) != 0)
				goto err_closedir;
			continue;
		}

		if ((fp = fopen(fname, "r")) == NULL) {
			dt_dprintf("skipping library %s: %s\n",
				   fname, strerror(errno));
			continue;
		}

		dtp->dt_filetag = fname;
		if (dt_lib_depend_add(dtp, &dtp->dt_lib_dep, fname) != 0)
			goto err_close;

		rv = dt_compile(dtp, DT_CTX_DPROG, DTRACE_PROBESPEC_NAME, NULL,
				DTRACE_C_EMPTY | DTRACE_C_CTL, 0, NULL, fp,
				NULL);

		if (rv != NULL && dtp->dt_errno &&
		     (dtp->dt_errno != EDT_COMPILER ||
		      dtp->dt_errtag != dt_errtag(D_PRAGMA_DEPEND)))
			goto err_close;

		if (dtp->dt_errno)
			dt_dprintf("error parsing library %s: %s\n", fname,
				   dtrace_errmsg(dtp, dtrace_errno(dtp)));

		fclose(fp);
		dtp->dt_filetag = NULL;
	}

	closedir(dirp);

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
		fclose(fp);
		dtp->dt_filetag = NULL;

		if (pgp == NULL &&
		    (dtp->dt_errno != EDT_COMPILER ||
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
	return 0;

err_close:
	fclose(fp);
err_closedir:
	closedir(dirp);
err:
	dt_lib_depend_free(dtp);
	return -1; /* preserve dt_errno */
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

	closedir(dirp);
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
		return 0; /* libraries already processed */

	dtp->dt_cflags |= DTRACE_C_NOLIBS;

	for (dirp = dt_list_next(&dtp->dt_lib_path); dirp != NULL;
	     dirp = dt_list_next(dirp)) {
		char *kdir_path;

		/* Load libs from per-kernel path if available. */
		kdir_path = dt_find_kernpath(dtp, dirp->dir_path);
		if (kdir_path != NULL) {
			if (dt_load_libs_dir(dtp, kdir_path) != 0) {
				dtp->dt_cflags &= ~DTRACE_C_NOLIBS;
				free(kdir_path);
				return -1;
			}
			free(kdir_path);
		}

		/* Load libs from original path in the list. */
		if (dt_load_libs_dir(dtp, dirp->dir_path) != 0) {
			dtp->dt_cflags &= ~DTRACE_C_NOLIBS;
			return -1; /* errno is set for us */
		}
	}

	return 0;
}
