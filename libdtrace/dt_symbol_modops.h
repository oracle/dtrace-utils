/*
 * Oracle Linux DTrace.
 * Copyright (c) 2009, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef BITS
#error This file should only be repeatedly #included by dt_module.c.
#endif

#define JOIN(pre,post) pre##post
#define EXJOIN(pre,post) JOIN(pre,post)
#define JOINMID(pre,mid,post) pre##mid##post
#define EXJOINMID(pre,mid,post) JOINMID(pre,mid,post)
#define BITIZE(pre) EXJOIN(pre,BITS)
#define ElfIZE(suffix) EXJOINMID(Elf,BITS,_##suffix)
#define ELFIZE(suffix) EXJOINMID(ELF,BITS,_##suffix)

static uint_t
BITIZE(dt_module_syminit)(dt_module_t *dmp)
{
	const ElfIZE(Sym) *sym = dmp->dm_symtab.cts_data;
	const char *base = dmp->dm_strtab.cts_data;
	size_t ss_size = dmp->dm_strtab.cts_size;
	uint_t i, n = dmp->dm_nsymelems;
	uint_t asrsv = 0;

	for (i = 0; i < n; i++, sym++) {
		const char *name = base + sym->st_name;
		uchar_t type = ELFIZE(ST_TYPE)(sym->st_info);

		if (type >= STT_NUM || type == STT_SECTION)
			continue; /* skip sections and unknown types */

		if (sym->st_name == 0 || sym->st_name >= ss_size)
			continue; /* skip null or invalid names */

		if (sym->st_value != 0 &&
		    (ELFIZE(ST_BIND)(sym->st_info) != STB_LOCAL || sym->st_size))
			asrsv++; /* reserve space in the address map */

		dt_module_symhash_insert(dmp, name, i);
	}

	return asrsv;
}

/*
 * Sort comparison function for N-bit symbol address-to-name lookups.
 */
static int
BITIZE(dt_module_symcomp)(const void *lp, const void *rp, void *strtabp)
{
	ElfIZE(Sym) *lhs = *((ElfIZE(Sym) **)lp);
	ElfIZE(Sym) *rhs = *((ElfIZE(Sym) **)rp);
	const char *strtab = strtabp;

	/* first sort by value */
	if (lhs->st_value < rhs->st_value)
		return -1;
	if (lhs->st_value > rhs->st_value)
		return 1;

	/* zero-size markers come before nonzero-size symbols */
	if ((lhs->st_size == 0) && (rhs->st_size != 0))
		return -1;
	if ((lhs->st_size != 0) && (rhs->st_size == 0))
		return +1;

	/* sort by type */
	if ((ELFIZE(ST_TYPE)(lhs->st_info) == STT_NOTYPE) !=
	    (ELFIZE(ST_TYPE)(rhs->st_info) == STT_NOTYPE))
		return ELFIZE(ST_TYPE)(lhs->st_info) == STT_NOTYPE ? 1 : -1;

	/* not weak */
	if ((ELFIZE(ST_BIND)(lhs->st_info) == STB_WEAK) !=
	    (ELFIZE(ST_BIND)(rhs->st_info) == STB_WEAK))
		return ELFIZE(ST_BIND)(lhs->st_info) == STB_WEAK ? 1 : -1;

	/* lexical order */
	return strcmp(strtab + lhs->st_name, strtab + rhs->st_name);
}

static void
BITIZE(dt_module_symsort)(dt_module_t *dmp)
{
	ElfIZE(Sym) *symtab = (ElfIZE(Sym) *)dmp->dm_symtab.cts_data;
	ElfIZE(Sym) **sympp = (ElfIZE(Sym) **)dmp->dm_asmap;
	const dt_modsym_t *dmsp = dmp->dm_symchains + 1;
	uint_t i, n = dmp->dm_symfree;

	for (i = 1; i < n; i++, dmsp++) {
		ElfIZE(Sym) *sym = symtab + dmsp->dms_symid;
		if (sym->st_value != 0 &&
		    (ELFIZE(ST_BIND)(sym->st_info) != STB_LOCAL || sym->st_size))
			*sympp++ = sym;
	}

	dmp->dm_aslen = (uint_t)(sympp - (ElfIZE(Sym) **)dmp->dm_asmap);
	assert(dmp->dm_aslen <= dmp->dm_asrsv);

	qsort_r(dmp->dm_asmap, dmp->dm_aslen,
	    sizeof(ElfIZE(Sym) *), BITIZE(dt_module_symcomp),
	    (void *)dmp->dm_strtab.cts_data);
}

static GElf_Sym *
BITIZE(dt_module_symname)(dt_module_t *dmp, const char *name,
    GElf_Sym *symp, uint_t *idp)
{
	const ElfIZE(Sym) *symtab = dmp->dm_symtab.cts_data;
	const char *strtab = dmp->dm_strtab.cts_data;

	const ElfIZE(Sym) *sym;
	const dt_modsym_t *dmsp;
	uint_t i, h;

	if (dmp->dm_nsymelems == 0)
		return NULL;

	h = str2hval(name, 0) % dmp->dm_nsymbuckets;

	for (i = dmp->dm_symbuckets[h]; i != 0; i = dmsp->dms_next) {
		dmsp = &dmp->dm_symchains[i];
		sym = symtab + dmsp->dms_symid;

		if (strcmp(name, strtab + sym->st_name) == 0) {
			if (idp != NULL)
				*idp = dmsp->dms_symid;
			return BITIZE(dt_module_symgelf)(sym, symp);
		}
	}

	return NULL;
}

static GElf_Sym *
BITIZE(dt_module_symaddr)(dt_module_t *dmp, GElf_Addr addr,
    GElf_Sym *symp, uint_t *idp)
{
	const ElfIZE(Sym) **asmap = (const ElfIZE(Sym) **)dmp->dm_asmap;
	const ElfIZE(Sym) *symtab = dmp->dm_symtab.cts_data;
	const ElfIZE(Sym) *sym;

	uint_t i, mid, lo = 0, hi = dmp->dm_aslen - 1;
	ElfIZE(Addr) v;

	if (dmp->dm_aslen == 0)
		return NULL;

	while (hi - lo > 1) {
		mid = (lo + hi) / 2;
		if (addr >= asmap[mid]->st_value)
			lo = mid;
		else
			hi = mid;
	}

	i = addr < asmap[hi]->st_value ? lo : hi;
	sym = asmap[i];
	v = sym->st_value;

	/*
	 * If the previous entry has the same value, improve our choice.  The
	 * order of equal-valued symbols is determined by the comparison func.
	 */
	while (i-- != 0 && asmap[i]->st_value == v)
		sym = asmap[i];

	if (addr - sym->st_value < MAX(sym->st_size, 1)) {
		if (idp != NULL)
			*idp = (uint_t)(sym - symtab);
		return BITIZE(dt_module_symgelf)(sym, symp);
	}

	return NULL;
}

static const dt_modops_t BITIZE(dt_modops_) = {
	BITIZE(dt_module_syminit),
	BITIZE(dt_module_symsort),
	BITIZE(dt_module_symname),
	BITIZE(dt_module_symaddr)
};

#undef JOIN
#undef EXJOIN
#undef JOINMID
#undef EXJOINMID
#undef BITIZE
#undef ElfIZE
#undef ELFIZE
