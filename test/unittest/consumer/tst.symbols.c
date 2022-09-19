/*
 * Oracle Linux DTrace.
 * Copyright (c) 2018, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * Check mappings between symbol names and addresses using /proc/kallmodsyms.
 */

/* @@timeout: 60 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dtrace.h>

int nchecks = 0, nerrors = 0;

typedef struct mysymbol {
	long long unsigned addr;
	long long unsigned size;
	char *symname;
	char *modname;
	char type;
	int is_duplicate;	/* same modname and symname */
} mysymbol_t;

mysymbol_t *symbols = NULL;
int maxnsymbols = 0, nsymbols = 0;

int mycompare_name(const void *ap, const void *bp)
{
	mysymbol_t *a = (mysymbol_t *)ap;
	mysymbol_t *b = (mysymbol_t *)bp;
	int c = strcmp(a->modname, b->modname);

	if (c)
		return c;
	return strcmp(a->symname, b->symname);
}

int mycompare_addr_size_type(const void *ap, const void *bp)
{
	mysymbol_t *a = (mysymbol_t *)ap;
	mysymbol_t *b = (mysymbol_t *)bp;
	if (a->addr < b->addr)
		return -1;
	if (a->addr > b->addr)
		return +1;
	if (a->size < b->size)
		return -1;
	if (a->size > b->size)
		return +1;
	if (a->type < b->type)
		return +1;
	if (a->type > b->type)
		return -1;
	return strcmp(a->symname, b->symname);
}

int read_symbols() {
	char *line = NULL;
	size_t line_n = 0;
	FILE *fd;
	int n_skip = 0, n_absolute = 0;
	unsigned int kernel_flag = 0;

	printf("read_symbols():\n");
	if ((fd = fopen("/proc/kallmodsyms", "r")) == NULL) return 1;
	while ((getline(&line, &line_n, fd)) > 0) {
		char symname[256];
		char modname[256] = "vmlinux]";

		/*
		 * Need to make more room.
		 */
		if (nsymbols >= maxnsymbols) {
			maxnsymbols *= 2;
			if (maxnsymbols == 0)
				maxnsymbols = 128 * 1024;
			symbols = (mysymbol_t *)realloc(symbols,
                            maxnsymbols * sizeof(mysymbol_t));
			if (symbols == NULL) {
				printf("ERROR: could not allocate symbols\n");
				fclose(fd);
				return 1;
			}
                }

		/*
		 * Read the symbol.
		 */
		if (sscanf(line, "%llx %llx %c %s [%s",
		    &symbols[nsymbols].addr,
		    &symbols[nsymbols].size,
		    &symbols[nsymbols].type,
		    symname, modname) < 4) {
			printf("ERROR: expected at least 4 fields\n");
			fclose(fd);
			return 1;
		}

		/*
		 * See dt_module.c, dt_modsym_update().
		 */
		if (symbols[nsymbols].type == 'a' ||
		    symbols[nsymbols].type == 'A')
			continue;
#define KERNEL_FLAG_INIT_SCRATCH 4
		if (strcmp(symname, "__init_scratch_begin") == 0) {
			kernel_flag |= KERNEL_FLAG_INIT_SCRATCH;
			continue;
		} else if (strcmp(symname, "__init_scratch_end") == 0) {
			kernel_flag &= ~ KERNEL_FLAG_INIT_SCRATCH;
			continue;
		} else if (kernel_flag & KERNEL_FLAG_INIT_SCRATCH) {
			continue;
		}
#undef KERNEL_FLAG_INIT_SCRATCH
		if (strcmp(modname, "bpf]") == 0)
			continue;

		if (strcmp(modname, "__builtin__ftrace]") == 0)
			continue;

		/*
		 * In libdtrace/dt_module.c function dt_modsym_update(),
		 * we skip a number of symbols.  Do not test them.  The
		 * list of skipped symbols might change over time.
		 */
#define strstarts(x) (strncmp(symname, x, strlen (x)) == 0)
		if (0
		    || strstarts("__crc_")        /* never triggers? */
		    || strstarts("__ksymtab_")
		    || strstarts("__kcrctab_")
		    || strstarts("__kstrtab_")
		    || strstarts("__param_")
		    || strstarts("__syscall_meta__")
		    || strstarts("__p_syscall_meta__")
		    || strstarts("__event_")
		    || strstarts("event_")
		    || strstarts("ftrace_event_")
		    || strstarts("types__")
		    || strstarts("args__")
		    || strstarts("__tracepoint_")
		    || strstarts("__tpstrtab_")
		    || strstarts("__tpstrtab__")
		    || strstarts("__initcall_")
		    || strstarts("__setup_")
		    || strstarts("__pci_fixup_")
		    ) {
			n_skip++;
			continue;
		}
#undef strstarts

		modname[strlen(modname)-1] = '\0';
		symbols[nsymbols].symname = strdup(symname);
		symbols[nsymbols].modname = strdup(modname);
		nsymbols += 1;
	}
	free(line);
	fclose(fd);
	printf("  %6d symbols kept\n", nsymbols);
	printf("  %6d symbols skipped\n", n_skip);
	return 0;
}

void print_symbol(int i) {
	printf(" %16.16llx %8.8llx %32s %20s\n",
	    (long long)symbols[i].addr,
	    (long long)symbols[i].size,
	    symbols[i].symname,
	    symbols[i].modname);
}

void init_is_duplicate() {
	int i;

	/* this should not happen */
	if (nsymbols <= 0)
		return;

	symbols[0].is_duplicate = 0;

	/* this should not happen */
	if (nsymbols == 1)
		return;

	/* handle the first symbol specially */
	if (strcmp(symbols[0].modname, symbols[1].modname) == 0 &&
	    strcmp(symbols[0].symname, symbols[1].symname) == 0)
		symbols[0].is_duplicate = 1;

	/* now handle all other comparisons */
	for (i = 1; i < nsymbols; i++) {
		if (strcmp(symbols[i - 1].modname, symbols[i].modname) == 0 &&
		    strcmp(symbols[i - 1].symname, symbols[i].symname) == 0) {
			symbols[i - 1].is_duplicate = 1;
			symbols[i].is_duplicate = 1;
		} else
			symbols[i].is_duplicate = 0;
	}
}

void match_at_addr(int i, GElf_Sym *symp, dtrace_syminfo_t *sip) {
	int j, match = -1;

	/*
	 * We looked up the symbol by address (of symbol i).
	 * So check the symbol address.
	 */
	if (symbols[i].addr != symp->st_value) {
		nerrors++;
		printf("ERROR: mismatch address\n");
		printf("  expect:");
		print_symbol(i);
		printf("  actual: %16.16llx %8.8llx %32s %20s\n",
		    (long long)symp->st_value,
		    (long long)symp->st_size,
		    sip->name,
		    sip->object);
	}

	/*
	 * Ideally, the found symbol should match symbol i completely, but
	 * "lookup by address" can be ambiguous.  So just see if we can match
	 * some known symbol at this address.
	 */

	if (strcmp(symbols[i].symname, sip->name) == 0)
		match = i;

	j = i - 1;
	while (match < 0 && j >= 0 && symbols[j].addr == symbols[i].addr) {
		if (strcmp(symbols[j].symname, sip->name) == 0)
			match = j;
		j--;
	}

	j = i + 1;
	while (match < 0 && j < nsymbols && symbols[j].addr == symbols[i].addr) {
		if (strcmp(symbols[j].symname, sip->name) == 0)
			match = j;
		j++;
	}

	/*
	 * Check that we found a symbol with the right address and name
	 * and that it also matches what we expect on size and module.
	 */

	if (match < 0 ||
	    symbols[match].size != symp->st_size ||
	    strcmp(symbols[match].modname, sip->object)) {
		nerrors++;
		printf("ERROR: no such reported symbol\n");
		printf("  expect:");
		print_symbol(i);
		printf("  actual: %16.16llx %8.8llx %32s %20s\n",
		    (long long)symp->st_value,
		    (long long)symp->st_size,
		    sip->name,
		    sip->object);
	}
}

int check_lookup_by_addr(dtrace_hdl_t *h) {
	int i, n_zero = 0;
	GElf_Sym sym;
	dtrace_syminfo_t si;

	printf("check look up symbol by address:\n");
	for (i = 0; i < nsymbols; i++) {
		/*
		 * In libdtrace/dt_module.c function dtrace_addr_range_cmp(),
		 * it is impossible for an address to match a range that has
		 * zero size.  So skip those symbols here.
		 */
		if (symbols[i].size == 0) {
			n_zero++;
			continue;
		}

		nchecks++;
		if (dtrace_lookup_by_addr
		    (h, (GElf_Addr)symbols[i].addr, &sym, &si)) {

			printf("ERROR: dtrace_lookup_by_addr failed: %s\n",
			    dtrace_errmsg(h, dtrace_errno(h)));
			printf("  expect:");
			print_symbol(i);
			nerrors++;
		} else {
			match_at_addr(i, &sym, &si);
		}
	}
	printf("  %6d symbols not checked due to zero size\n", n_zero);
}

int check_lookup_by_name(dtrace_hdl_t *h, int specify_module) {
	int i, n_dupl = 0;
	GElf_Sym sym;
	dtrace_syminfo_t si;

	printf("check lookup by symbol name%s:\n",
	    specify_module ? " and module" : "");
	for (i = 0; i < nsymbols; i++) {
		nchecks++;

		if (dtrace_lookup_by_name(h,
		    specify_module ? symbols[i].modname : DTRACE_OBJ_KMODS,
		    symbols[i].symname, &sym, &si)) {
			printf("ERROR: dtrace_lookup_by_name failed\n");
			printf("  expect:");
			print_symbol(i);
			nerrors++;
		} else {

			if (symbols[i].addr != sym.st_value ||
			    symbols[i].size != sym.st_size ||
			    strcmp(symbols[i].symname, si.name) ||
			    strcmp(symbols[i].modname, si.object)) {

				if (symbols[i].is_duplicate) {
					n_dupl++;
					continue;
				}

				printf("ERROR: mismatch\n");
				printf("  expect:");
				print_symbol(i);
				printf("  actual: %16.16llx %8.8llx %32s %20s\n",
				    (long long)sym.st_value,
				    (long long)sym.st_size,
				    si.name,
				    si.object);
				nerrors++;
			}
		}
	}
	printf("  %6d symbols not checked due to duplicate name\n", n_dupl);
}

int main(int argc, char **argv) {
	int err;
	dtrace_hdl_t *h = dtrace_open(DTRACE_VERSION, 0, &err);
	if (h == NULL) {
		printf("ERROR: dtrace_open %d |%s|\n",
		    err, dtrace_errmsg(h, err));
		return 1;
	}

	if (read_symbols() != 0)
		return 1;

	/* look for duplicates by name */
	qsort(symbols, nsymbols, sizeof(mysymbol_t), &mycompare_name);
	init_is_duplicate();

	/* sort to facilitate checking by-address matches */
	qsort(symbols, nsymbols, sizeof(mysymbol_t), &mycompare_addr_size_type);

	check_lookup_by_addr(h);
	check_lookup_by_name(h, 1);
/*	check_lookup_by_name(h, 0); */ /* do not expect this to pass */

	dtrace_close(h);

	printf("test tst.symbols.c had %d errors (out of %d)\n",
	    nerrors, nchecks);
	return nerrors != 0;
}
