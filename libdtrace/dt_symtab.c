/*
 * Symbol table support for DTrace.
 *
 * We cannot rely on ELF symbol table management at all times: in particular,
 * kernel symbols have no ELF symbol table.  Thus, this module implements a
 * simple, reasonably memory-efficient symbol table manager.
 *
 * TODO: increase efficiency (perhaps Huffman-coding symbol names?)
 */

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
 * Copyright 2012 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <stdlib.h>
#include <string.h>
#include <dt_symtab.h>
#include <dt_impl.h>
#include <dt_list.h>
#include <unistd.h>

#define DT_ST_SORTED 0x01		/* Sorted, ready for searching. */
#define DT_ST_PACKED 0x02		/* Symbol table packed
					 * (necessarily sorted too) */

struct dt_symbol {
	dt_list_t dts_list;		/* list forward/back pointers */
	union {
		char *str;		/* symbol name */
		size_t off;		/* symbol offset in strtab */
	} dts_name;
	GElf_Addr dts_addr;		/* symbol address */
	GElf_Xword dts_size;		/* symbol size */
	unsigned char dts_info;		/* ELF symbol information */
	struct dt_symbol *dts_next;	/* hash chain pointer */
};

struct dt_symtab {
	dt_list_t dtst_symlist;		/* symbol list */
	dt_symbol_t **dtst_syms_by_name;/* symbol name->addr hash buckets */
	uint_t dtst_symbuckets;		/* number of buckets */
	uint_t dtst_nsyms;		/* number of symbols */
	char *dtst_strtab;		/* string table of symbol names */
	dt_symbol_t **dtst_addrs;	/* sorted address->symbol mapping */
	uint_t dtst_alloc_addrs;	/* number of allocated addrs */
	int dtst_flags;			/* symbol table flags */
};

/*
 * Grow the address->symbol mapping, zeroing out the new elements.
 */
static dt_symbol_t **
dt_symtab_grow_addrs(dt_symtab_t *symtab)
{
	uint_t alloc_addrs = symtab->dtst_alloc_addrs;
	dt_symbol_t **new_addrs;

	alloc_addrs = (alloc_addrs + 1) * 2;
	new_addrs = realloc(symtab->dtst_addrs,
	    sizeof (struct dt_symbol *) * alloc_addrs);

	if (new_addrs == NULL)
		return NULL;

	bzero(new_addrs + symtab->dtst_alloc_addrs,
	    (alloc_addrs - symtab->dtst_alloc_addrs) *
	    sizeof (struct dt_symbol *));
	symtab->dtst_alloc_addrs = alloc_addrs;
	symtab->dtst_addrs = new_addrs;

	return new_addrs;
}

/*
 * Sort symbols in dtst_addrs into order.
 *
 * As with dt_module_symcomp(), we order identically-addressed symbols first if
 * they are non-zero-sized, typed, non-weak, or lexically first, in that order.
 * (It is theoretically possible to have two identical symbols with identical
 * addresses coming from different modules, but since this code does not track
 * modules it doesn't need to care.)
 *
 * We do not bother to handle the pathological case of symbols with distinct
 * addresses which nonetheless overlap: the assumption is that if N symbols span
 * an address, they all have the same starting address and size.
 */
static int
dt_symbol_sort_cmp(const void *lp, const void *rp)
{
	const dt_symbol_t * const *lhsp =  lp;
	const dt_symbol_t * const *rhsp = rp;
	const dt_symbol_t *lhs = *lhsp;
	const dt_symbol_t *rhs = *rhsp;

	if (lhs->dts_addr != rhs->dts_addr)
		return lhs->dts_addr > rhs->dts_addr;

	if ((GELF_ST_TYPE(lhs->dts_info) == STT_NOTYPE) !=
	    (GELF_ST_TYPE(rhs->dts_info) == STT_NOTYPE))
		return GELF_ST_TYPE(lhs->dts_info) == STT_NOTYPE ? 1 : -1;

	if ((GELF_ST_BIND(lhs->dts_info) == STB_WEAK) !=
	    (GELF_ST_BIND(rhs->dts_info) == STB_WEAK))
		return GELF_ST_BIND(lhs->dts_info) == STB_WEAK ? 1 : -1;

	/*
	 * Note: packed strtabs must already be sorted and cannot be changed nor
	 * resorted.
	 */
	return (strcmp(lhs->dts_name.str, rhs->dts_name.str));
}

/*
 * Find some symbol in dtst_addrs spanning the desired address.
 *
 * If several symbols span that address, return an arbitrary one.
 */
static int dt_symbol_search_cmp(const void *lp, const void *rp)
{
	const GElf_Addr *lhsp = lp;
	dt_symbol_t * const *rhsp = rp;
	const GElf_Addr lhs = *lhsp;
	const dt_symbol_t *rhs = *rhsp;

	if ((lhs >= rhs->dts_addr) && (lhs < rhs->dts_addr + rhs->dts_size))
		return 0;
	else if (lhs < rhs->dts_addr)
		return -1;
	else
		return 1;
}

dt_symtab_t *
dt_symtab_create(void)
{
	dt_symtab_t *symtab = malloc (sizeof (struct dt_symtab));

	if (symtab == NULL)
		return NULL;

	bzero(symtab, sizeof (struct dt_symtab));

	symtab->dtst_symbuckets = _dtrace_strbuckets;
	symtab->dtst_syms_by_name = calloc(symtab->dtst_symbuckets,
	    sizeof (struct dt_symbol *));

	if (symtab->dtst_syms_by_name == NULL) {
		free(symtab->dtst_syms_by_name);
		free(symtab);
		return NULL;
	}

	return symtab;
}

void
dt_symtab_destroy(dt_symtab_t *symtab)
{
	dt_symbol_t *dtsp;
	dt_symbol_t *last_dtsp = NULL;

	if (!symtab)
		return;

	free(symtab->dtst_addrs);
	free(symtab->dtst_syms_by_name);
	free(symtab->dtst_strtab);

	for (dtsp = dt_list_next(&symtab->dtst_symlist); dtsp != NULL;
	     dtsp = dt_list_next(dtsp)) {
		if (!(symtab->dtst_flags & DT_ST_PACKED))
			free(dtsp->dts_name.str);
		free(last_dtsp);
		last_dtsp = dtsp;
	}
	free(last_dtsp);

	free(symtab);
}

dt_symbol_t *
dt_symbol_insert(dt_symtab_t *symtab, const char *name,
    GElf_Addr addr, GElf_Xword size, unsigned char info)
{
	uint_t h;
	dt_symbol_t **symaddr;
	dt_symbol_t *dtsp;

	/*
	 * No insertion into packed symtabs.
	 */
	if (symtab->dtst_flags & DT_ST_PACKED)
		return NULL;

	if ((dtsp = dt_symbol_by_name(symtab, name)) != NULL)
		return dtsp;

	if ((dtsp = malloc(sizeof (dt_symbol_t))) == NULL)
		return NULL;

	if (symtab->dtst_nsyms >= symtab->dtst_alloc_addrs)
		if (dt_symtab_grow_addrs(symtab) == NULL) {
			free(dtsp);
			return NULL;
		}

	bzero(dtsp, sizeof (dt_symbol_t));
	dtsp->dts_name.str = strdup(name);
	dtsp->dts_addr = addr;
	dtsp->dts_size = size;
	dtsp->dts_info = info;

	if (dtsp->dts_name.str == NULL) {
		free(dtsp->dts_name.str);
		free(dtsp);
		return NULL;
	}

	/*
	 * Address->symbol mapping.
	 */

	symaddr = &symtab->dtst_addrs[symtab->dtst_nsyms];
	*symaddr = dtsp;

	/*
	 * Finalize; chain into the list and hash table, increase symbol count.
	 */
	dt_list_append(&symtab->dtst_symlist, dtsp);
	symtab->dtst_nsyms++;

	h = dt_strtab_hash(name, NULL) % symtab->dtst_symbuckets;
	dtsp->dts_next = symtab->dtst_syms_by_name[h];
	symtab->dtst_syms_by_name[h] = dtsp;

	symtab->dtst_flags &= ~DT_ST_SORTED;

	return dtsp;
}

dt_symbol_t *
dt_symbol_by_name(dt_symtab_t *symtab, const char *name)
{
	uint_t h = dt_strtab_hash(name, NULL) % symtab->dtst_symbuckets;
	dt_symbol_t *dtsp;
	int packed = symtab->dtst_flags & DT_ST_PACKED;

	for (dtsp = symtab->dtst_syms_by_name[h]; dtsp != NULL;
	     dtsp = dtsp->dts_next) {
		if (packed) {
			if (strcmp(&symtab->dtst_strtab[dtsp->dts_name.off],
				name) == 0)
				return (dtsp);
		}
		else
			if (strcmp(dtsp->dts_name.str, name) == 0)
				return (dtsp);
	}

	return NULL;
}

dt_symbol_t *
dt_symbol_by_addr(dt_symtab_t *symtab, GElf_Addr dts_addr)
{
	dt_symbol_t **sympp, *lastsymp;

	if (symtab->dtst_addrs == NULL)
		return NULL;

	if (!(symtab->dtst_flags & DT_ST_SORTED))
		return NULL;

	/*
	 * After finding a symbol spanning the desired address, hunt backwards
	 * for the first such symbol: it is the most desirable one.
	 */

	sympp = bsearch(&dts_addr, symtab->dtst_addrs, symtab->dtst_nsyms,
	    sizeof (struct dt_symbol *), dt_symbol_search_cmp);

	if (sympp == NULL)
		return NULL;

	for (lastsymp = *sympp;
	     (sympp > symtab->dtst_addrs) &&
		 ((*sympp)->dts_addr <= dts_addr) &&
		 ((*sympp)->dts_addr + (*sympp)->dts_size > dts_addr);
	     lastsymp = *sympp, sympp--)
		;

	if ((lastsymp->dts_addr <= dts_addr) &&
	    (lastsymp->dts_addr + lastsymp->dts_size > dts_addr))
		return lastsymp;

	return NULL;
}

void
dt_symtab_sort(dt_symtab_t *symtab)
{
	if (symtab->dtst_flags & DT_ST_SORTED)
		return;

	qsort(symtab->dtst_addrs, symtab->dtst_nsyms,
	    sizeof (dt_symbol_t *), dt_symbol_sort_cmp);

	symtab->dtst_flags |= DT_ST_SORTED;
}

void
dt_symtab_pack(dt_symtab_t *symtab)
{
	dt_symbol_t *dtsp;
	size_t strsz = 0, offset = 0;

	/*
	 * For now, merely pack the symbols into a string table.
	 *
	 * In future, Huffman-coding the symbols seems sensible: they have many
	 * identical components (a property which scripts/kallsyms.c also takes
	 * advantage of).
	 */

	if (symtab->dtst_flags & DT_ST_PACKED)
		return;

	dt_symtab_sort(symtab);

	/*
	 * Size and allocate the string table: give up if we can't.
	 */
	for (dtsp = dt_list_next(&symtab->dtst_symlist); dtsp != NULL;
	     dtsp = dt_list_next(dtsp))
		strsz += strlen(dtsp->dts_name.str) + 1;

	symtab->dtst_strtab = malloc(strsz);
	if (symtab->dtst_strtab == NULL)
		return;

	/*
	 * Fill it out.
	 */
	for (dtsp = dt_list_next(&symtab->dtst_symlist); dtsp != NULL;
	     dtsp = dt_list_next(dtsp)) {
		size_t len = strlen(dtsp->dts_name.str) + 1;

		memcpy(&symtab->dtst_strtab[offset], dtsp->dts_name.str, len);
		free(dtsp->dts_name.str);
		dtsp->dts_name.off = offset;
		offset += len;
	}

	symtab->dtst_flags |= DT_ST_PACKED;
}

/*
 * Return the name of a symbol.  Currently redundant, this will become useful
 * when dt_symtab_pack() starts compressing symbol names.  TODO: we must retain
 * ownership of the expanded string, even in this case: it is part of the
 * libdtrace API.  This is distinctly tricky: it may be simpler to change the
 * API to require caller-frees semantics.
 */
const char *
dt_symbol_name(dt_symtab_t *symtab _dt_unused_, dt_symbol_t *symbol)
{
	if (symtab->dtst_flags & DT_ST_PACKED)
		return &symtab->dtst_strtab[symbol->dts_name.off];
	else
		return symbol->dts_name.str;
}

void
dt_symbol_to_elfsym64(dtrace_hdl_t *dtp, dt_symbol_t *symbol, Elf64_Sym *elf_symp)
{
	elf_symp->st_info = symbol->dts_info;
	elf_symp->st_value = symbol->dts_addr;
	elf_symp->st_size = symbol->dts_size;
	elf_symp->st_shndx = 1; /* 'not SHN_UNDEF' is all we guarantee */
}

void
dt_symbol_to_elfsym32(dtrace_hdl_t *dtp, dt_symbol_t *symbol, Elf32_Sym *elf_symp)
{
	elf_symp->st_info = symbol->dts_info;
	elf_symp->st_value = symbol->dts_addr;
	elf_symp->st_size = symbol->dts_size;
	elf_symp->st_shndx = 1; /* 'not SHN_UNDEF' is all we guarantee */
}

void
dt_symbol_to_elfsym(dtrace_hdl_t *dtp, dt_symbol_t *symbol, GElf_Sym *elf_symp)
{
	switch (dtp->dt_conf.dtc_ctfmodel) {
	case CTF_MODEL_LP64: dt_symbol_to_elfsym64(dtp, symbol, (Elf64_Sym *) elf_symp);
		break;
	case CTF_MODEL_ILP32: dt_symbol_to_elfsym32(dtp, symbol, (Elf32_Sym *)elf_symp);
		break;
	default:;
		/* unknown model, fall out with nothing changed */
	}
}
