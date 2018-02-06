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
 * Oracle Linux DTrace.
 * Copyright (c) 2012, 2018, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
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

/*
 * Symbol address ranges might overlap.  E.g., one symbol might have a broad
 * range, while another spans a subset of that range.  The address-to-symbol
 * mapping should return the narrowest symbol.  So the narrower symbol will
 * be returned for middle addresses and the broader symbol for lower and
 * higher addresses.
 *
 * Here we define symbol address ranges for the address-to-symbol translation.
 * Note that a symbol might end up with more than one range.
 */
typedef struct dt_symrange {
	GElf_Addr dtsr_lo;
	GElf_Addr dtsr_hi;
	dt_symbol_t *dtsr_sym;
} dt_symrange_t;

struct dt_symtab {
	dt_list_t dtst_symlist;		/* symbol list */
	dt_symbol_t **dtst_syms_by_name;/* symbol name->addr hash buckets */
	uint_t dtst_symbuckets;		/* number of buckets */
	char *dtst_strtab;		/* string table of symbol names */
	dt_symrange_t *dtst_ranges;	/* range->symbol mapping */
	uint_t dtst_num_range;		/*   - number of ranges */
	uint_t dtst_num_range_alloc;	/*   - number of ranges allocated */
	int dtst_flags;			/* symbol table flags */
};

/*
 * Grow the range->symbol mapping.
 */
static dt_symrange_t *
dt_symtab_grow_ranges(dt_symtab_t *symtab)
{
	uint_t num_alloc = (symtab->dtst_num_range_alloc + 1) * 2;
	dt_symrange_t *new_ranges = realloc(symtab->dtst_ranges,
	    sizeof (dt_symrange_t) * num_alloc);
	if (new_ranges == NULL)
		return NULL;

	symtab->dtst_num_range_alloc = num_alloc;
	symtab->dtst_ranges = new_ranges;

	return new_ranges;
}

/*
 * Sort dtst_ranges.
 *
 * Somewhat mimics dt_module_symcomp(), but note:
 * - here there should be no zero-size symbols
 * - we do sort by size to help identify aliases
 *     (different names but same addr and size)
 * - we demote the name "cleanup_module"
 */
static int
dt_symrange_sort_cmp(const void *lp, const void *rp)
{
	dt_symbol_t *lhs = ((dt_symrange_t *) lp)->dtsr_sym;
	dt_symbol_t *rhs = ((dt_symrange_t *) rp)->dtsr_sym;

	if (lhs->dts_addr < rhs->dts_addr)
		return -1;
	if (lhs->dts_addr > rhs->dts_addr)
		return +1;

	if (lhs->dts_size > rhs->dts_size)
		return -1;
	if (lhs->dts_size < rhs->dts_size)
		return +1;

	if ((GELF_ST_TYPE(lhs->dts_info) == STT_NOTYPE) !=
	    (GELF_ST_TYPE(rhs->dts_info) == STT_NOTYPE))
		return GELF_ST_TYPE(lhs->dts_info) == STT_NOTYPE ? 1 : -1;

	if ((GELF_ST_BIND(lhs->dts_info) == STB_WEAK) !=
	    (GELF_ST_BIND(rhs->dts_info) == STB_WEAK))
		return GELF_ST_BIND(lhs->dts_info) == STB_WEAK ? 1 : -1;

	/*
	 * Note: packed strtabs must already be sorted and can be
	 * neither changed nor resorted.
	 */
	if (strcmp(lhs->dts_name.str, "cleanup_module") &&
	    strcmp(rhs->dts_name.str, "cleanup_module") == 0)
		return -1;
	if (strcmp(rhs->dts_name.str, "cleanup_module") &&
	    strcmp(lhs->dts_name.str, "cleanup_module") == 0)
		return +1;
	return (strcmp(lhs->dts_name.str, rhs->dts_name.str));
}

/*
 * Find some symbol in dtst_ranges spanning the desired address.
 */
static int dt_symbol_search_cmp(const void *lp, const void *rp)
{
	const GElf_Addr lhs = *((GElf_Addr *) lp);
	dt_symrange_t *rhs = (dt_symrange_t *) rp;

	if (lhs < rhs->dtsr_lo)
		return -1;
	if (lhs >= rhs->dtsr_hi)
		return 1;
	return 0;
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

	free(symtab->dtst_ranges);
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
	dt_symbol_t *dtsp;

	/*
	 * No insertion into packed symtabs.
	 */
	if (symtab->dtst_flags & DT_ST_PACKED)
		return NULL;

	if ((dtsp = malloc(sizeof (dt_symbol_t))) == NULL)
		return NULL;

	if (symtab->dtst_num_range >= symtab->dtst_num_range_alloc)
		if (dt_symtab_grow_ranges(symtab) == NULL) {
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
	 * Address->symbol mapping.  Zero-size symbols do not
	 * include any addresses and therefore are not added.
	 * It is not yet necessary to set the range's addr and size.
	 */

	if (size > 0) {
		symtab->dtst_ranges[symtab->dtst_num_range].dtsr_sym = dtsp;
		symtab->dtst_num_range++;
	}

	/*
	 * Append to the doubly linked list.
	 */
	dt_list_append(&symtab->dtst_symlist, dtsp);

	/*
	 * Add to lookup-by-name hash table.
	 */
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
	dt_symrange_t *sympp;

	if (symtab->dtst_ranges == NULL)
		return NULL;

	if (!(symtab->dtst_flags & DT_ST_SORTED))
		return NULL;

	sympp = bsearch(&dts_addr, symtab->dtst_ranges, symtab->dtst_num_range,
	    sizeof (dt_symrange_t), dt_symbol_search_cmp);

	if (sympp == NULL)
		return NULL;

	return sympp->dtsr_sym;
}

static int
dt_symtab_form_ranges(dt_symtab_t *symtab)
{
	/*
	 * At this point, the addresses are sorted, but the ranges they
	 * represent could possibly overlap.  We want to form new ranges,
	 * so that each range maps to only one symbol.  Consequently, a
	 * symbol might be represented by zero, one, or many ranges.
	 *
	 * The algorithm for forming these ranges is intricate, but consider
	 * three symbols with these address ranges, with address increasing
	 * from left to right:
	 *
	 *              AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
	 *                           BBBBBBBBBBBBBBBB
	 *                           CCCCCCCCCCCCCCCC
	 *                                  DDDD
	 *
	 * Some addresses are spanned by as many as all four symbols A, B, C,
	 * and D.  We want to produce the following ranges:
	 *
	 *              AAAAAAAAAAAAA
	 *                           BBBBBBB
	 *                                  DDDD
	 *                                      BBBBB
	 *                                           AAAAAA
	 *
	 * Each range now maps to only one symbol.  The number of ranges may
	 * be different from the number of symbols.  Some symbols (like A and B)
	 * may be represented by multiple ranges and some (like C, which is an
	 * alias of B) not at all.
	 */
	dt_symrange_t *old_ranges = symtab->dtst_ranges;
	dt_symrange_t *new_ranges;
	uint_t num_alloc = symtab->dtst_num_range_alloc;
	uint_t num_range = 0;
	int i;
	GElf_Addr lo, hi = 0;

	if (symtab->dtst_num_range == 0)
		return 0;

	new_ranges = malloc(sizeof (dt_symrange_t) * num_alloc);

	if (new_ranges == NULL)
		return -1;

	/* now start forming address-to-symbol mapping, one symbol per range */

	hi = 0;
	i = 0;
	while (i < symtab->dtst_num_range) {
		int j;

		/* guess that the next range will be the next symbol */

		dt_symbol_t *sym = old_ranges[i].dtsr_sym;

		/*
		 * Set the low and high for this range.
		 * Check the previous high to decide whether:
		 *   - to crop the low end
		 *   - to move beyond this symbol altogether
		 */

		lo = sym->dts_addr;
		if (lo < hi) {
			lo = hi;
			if (sym->dts_addr + sym->dts_size <= hi) {
				i++;
				continue;
			}
		}
		hi = sym->dts_addr + sym->dts_size;

		/* check for other candidate symbols for this range */

		for (j = i + 1; j < symtab->dtst_num_range; j++) {
			dt_symbol_t *sym2 = old_ranges[j].dtsr_sym;
			GElf_Addr hi2;

			/* if sym2 is too high, all others will be as well */
			if (sym2->dts_addr >= hi)
				break;

			/* break range down if necessary */
			if (sym2->dts_addr > lo) {
				hi = sym2->dts_addr;
				break;
			}
			hi2 = sym2->dts_addr + sym2->dts_size;
			if (hi2 <= lo)
				continue;
			if (hi2 < hi)
				hi = hi2;

			/* decide whether sym2 should win over sym */
			if ((sym2->dts_addr > sym->dts_addr) ||
			    ((sym2->dts_addr == sym->dts_addr) &&
			    (sym2->dts_size < sym->dts_size)))
				sym = sym2;
		}

		/* check if we can coalese the new range to the last one */

		if (num_range > 0 &&
		    new_ranges[num_range-1].dtsr_hi == lo &&
		    new_ranges[num_range-1].dtsr_sym == sym)
			new_ranges[num_range-1].dtsr_hi = hi;
		else {

			/* ensure enough room (should not need much) */
			if (num_range >= num_alloc) {
				uint_t n = num_alloc + 1024;
				dt_symrange_t *r = realloc(new_ranges,
				    sizeof (dt_symrange_t) * n);
				if (r == NULL)
					return -1;

				num_alloc = n;
				new_ranges = r;
			}

			/* add the new range */

			new_ranges[num_range].dtsr_lo = lo;
			new_ranges[num_range].dtsr_hi = hi;
			new_ranges[num_range].dtsr_sym = sym;
			num_range++;
		}
	}

	free(symtab->dtst_ranges);
	symtab->dtst_num_range = num_range;
	symtab->dtst_num_range_alloc = num_alloc;
	symtab->dtst_ranges = new_ranges;
	return 0;
}

/*
 * Sort the address-to-name list.
 */
void
dt_symtab_sort(dt_symtab_t *symtab)
{
	if (symtab->dtst_flags & DT_ST_SORTED)
		return;

	qsort(symtab->dtst_ranges, symtab->dtst_num_range,
	    sizeof (dt_symrange_t), dt_symrange_sort_cmp);

	if (dt_symtab_form_ranges(symtab))
		return;

	symtab->dtst_flags |= DT_ST_SORTED;
}

/*
 * Get next item on the linked list, keeping or eliminating the current item.
 */
static
dt_symbol_t **
next_symp(dt_symbol_t **p, int *nelim, int keep) {
	if (keep)
		return &((*p)->dts_next);
	else {
		dt_symbol_t *tmp = (*p)->dts_next;
		(*p)->dts_next = NULL;
		*p = tmp;
		*nelim += 1;
		return p;
	}
}

/*
 * Purge symbols from name-to-address hash buckets if they have duplicates.
 *
 * (Duplicates occur when symbols have the same name and are in the same module;
 * they just come from different translation units.  For example, they might have
 * file scope and come from different files.)
 */
void
dt_symtab_purge(dt_symtab_t *symtab)
{
	uint_t i;

	/* loop over buckets */
	for (i = 0; i < symtab->dtst_symbuckets; i++) {

		/* walk the bucket's linked list */
		dt_symbol_t **p1;
		for (p1 = &symtab->dtst_syms_by_name[i]; *p1; ) {
			int nelim = 0;
			char *myname = (*p1)->dts_name.str;
			dt_symbol_t **p2;

			/*
			 * Walk from the next item to the end of the list,
			 * keeping only symbols whose names differ from myname.
			 * (Compare symbol names by looking at dts_name.str,
			 * since symtab is not packed yet.)
			 */
			for (p2 = &((*p1)->dts_next); *p2; )
				p2 = next_symp(p2, &nelim,
				    strcmp((*p2)->dts_name.str, myname));

			/*
			 * Advance p1, keeping the current item only if no
			 * other symbols were eliminated (duplicated p1).
			 */
			p1 = next_symp(p1, &nelim, nelim == 0);
		}
	}
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
