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
 * Copyright 2009 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
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

	return (asrsv);
}

/*
 * Sort comparison function for N-bit symbol address-to-name lookups.  We sort
 * symbols by value.  If values are equal, we prefer the symbol that is
 * non-zero sized, typed, not weak, or lexically first, in that order.
 */
static int
BITIZE(dt_module_symcomp)(const void *lp, const void *rp)
{
	ElfIZE(Sym) *lhs = *((ElfIZE(Sym) **)lp);
	ElfIZE(Sym) *rhs = *((ElfIZE(Sym) **)rp);

	if (lhs->st_value != rhs->st_value)
		return (lhs->st_value > rhs->st_value ? 1 : -1);

	if ((lhs->st_size == 0) != (rhs->st_size == 0))
		return (lhs->st_size == 0 ? 1 : -1);

	if ((ELFIZE(ST_TYPE)(lhs->st_info) == STT_NOTYPE) !=
	    (ELFIZE(ST_TYPE)(rhs->st_info) == STT_NOTYPE))
		return (ELFIZE(ST_TYPE)(lhs->st_info) == STT_NOTYPE ? 1 : -1);

	if ((ELFIZE(ST_BIND)(lhs->st_info) == STB_WEAK) !=
	    (ELFIZE(ST_BIND)(rhs->st_info) == STB_WEAK))
		return (ELFIZE(ST_BIND)(lhs->st_info) == STB_WEAK ? 1 : -1);

	return (strcmp(dt_module_strtab + lhs->st_name,
	    dt_module_strtab + rhs->st_name));
}

static void
BITIZE(dt_module_symsort)(dt_module_t *dmp)
{
	ElfIZE(Sym) *symtab = (ElfIZE(Sym) *)dmp->dm_symtab.cts_data;
	ElfIZE(Sym) **sympp = (ElfIZE(Sym) **)dmp->dm_asmap;
	const dt_sym_t *dsp = dmp->dm_symchains + 1;
	uint_t i, n = dmp->dm_symfree;

	for (i = 1; i < n; i++, dsp++) {
		ElfIZE(Sym) *sym = symtab + dsp->ds_symid;
		if (sym->st_value != 0 &&
		    (ELFIZE(ST_BIND)(sym->st_info) != STB_LOCAL || sym->st_size))
			*sympp++ = sym;
	}

	dmp->dm_aslen = (uint_t)(sympp - (ElfIZE(Sym) **)dmp->dm_asmap);
	assert(dmp->dm_aslen <= dmp->dm_asrsv);

	dt_module_strtab = dmp->dm_strtab.cts_data;
	qsort(dmp->dm_asmap, dmp->dm_aslen,
	    sizeof (ElfIZE(Sym) *), BITIZE(dt_module_symcomp));
	dt_module_strtab = NULL;
}

static GElf_Sym *
BITIZE(dt_module_symname)(dt_module_t *dmp, const char *name,
    GElf_Sym *symp, uint_t *idp)
{
	const ElfIZE(Sym) *symtab = dmp->dm_symtab.cts_data;
	const char *strtab = dmp->dm_strtab.cts_data;

	const ElfIZE(Sym) *sym;
	const dt_sym_t *dsp;
	uint_t i, h;

	if (dmp->dm_nsymelems == 0)
		return (NULL);

	h = dt_strtab_hash(name, NULL) % dmp->dm_nsymbuckets;

	for (i = dmp->dm_symbuckets[h]; i != 0; i = dsp->ds_next) {
		dsp = &dmp->dm_symchains[i];
		sym = symtab + dsp->ds_symid;

		if (strcmp(name, strtab + sym->st_name) == 0) {
			if (idp != NULL)
				*idp = dsp->ds_symid;
			return (BITIZE(dt_module_symgelf)(sym, symp));
		}
	}

	return (NULL);
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
		return (NULL);

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
		return (BITIZE(dt_module_symgelf)(sym, symp));
	}

	return (NULL);
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
