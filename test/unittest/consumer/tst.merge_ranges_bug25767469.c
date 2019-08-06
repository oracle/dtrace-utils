/*
 * Oracle Linux DTrace.
 * Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * 25767469 dtrace_update() does not merge address ranges properly
 * Verify fix.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dtrace.h>

typedef struct myrange{
	long unsigned addr;
	long unsigned size;
	char *name;
	char type;
} myrange_t;

myrange_t *ranges;
int maxnranges, nranges;

int mycompare(const void *ap, const void *bp)
{
	myrange_t *a = (myrange_t *) ap;
	myrange_t *b = (myrange_t *) bp;
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
	return strcmp(a->name, b->name);
}

void print_range(myrange_t *r)
{
	printf(" %c range %s %lx-%lx",
	    r->type, r->name, r->addr, r->addr + r->size);
}

int nerrors = 0;

/*
 * Look at the address ranges for a particular module name and type
 * (text 't' or data 'd').  A built-in module might not have a single
 * text or data segment.
 */

void check_module_ranges
(size_t n, dtrace_addr_range_t *addrs, const char *name, char type)
{
	int i;
	/*
	 * Check for a gap between consecutive ranges.
	 * The check ensures:
	 *   - ranges are in order
	 *   - ranges do not overlap or abut
	 *         (otherwise they should have been coalesced)
	 */
	for (i = 1; i < n; i++) {
		if ( addrs[i-1].dar_va
		    + addrs[i-1].dar_size
		    >= addrs[i].dar_va ) {
			printf("ERROR: %s %c range %lx-%lx"
			    " overlaps or abuts %lx-%lx\n", name, type,
			    addrs[i-1].dar_va,
			    addrs[i-1].dar_va + addrs[i-1].dar_size,
			    addrs[i].dar_va,
			    addrs[i].dar_va + addrs[i].dar_size);
			nerrors++;
		}
	}

	/*
	 * Record ranges so that we can later check
	 * all ranges from all modules against each other.
	 */
	for (i = 0; i < n; i++) {
		if (nranges == maxnranges) {
			maxnranges *= 4;
			ranges = (myrange_t *) realloc(ranges,
			    maxnranges * sizeof (myrange_t));
		}
		ranges[nranges].addr = addrs[i].dar_va;
		ranges[nranges].size = addrs[i].dar_size;
		ranges[nranges].name = strdup(name);
		ranges[nranges].type = type;
		nranges++;
	}
}

int mod_callback(dtrace_hdl_t * h, const dtrace_objinfo_t * d, void * arg) {
	if (arg) {
		/* when arg!=0, check the module ranges */
		check_module_ranges(d->dto_text_addrs_size,
		    d->dto_text_addrs, d->dto_name, 't');
		check_module_ranges(d->dto_data_addrs_size,
		    d->dto_data_addrs, d->dto_name, 'd');
	} else {
		/* when arg==0; dry run: look up info to force filename to load */
		GElf_Sym symp;
		dtrace_syminfo_t sip;
		if (d->dto_text_addrs_size) {
			dtrace_lookup_by_addr(h,
			    d->dto_text_addrs[0].dar_va, &symp, &sip);
		}
		if (d->dto_data_addrs_size) {
			dtrace_lookup_by_addr(h,
			    d->dto_data_addrs[0].dar_va, &symp, &sip);
		}
	}
	return(0);
}

int main(int argc, char **argv)
{
	int i, err;
	dtrace_hdl_t *h = dtrace_open(DTRACE_VERSION, 0, &err);
	if (h == NULL) {
		printf("ERROR: dtrace_open %d |%s|\n",
		    err, dtrace_errmsg(h, err));
		return 1;
	}

	/*
	 * initialize list of ranges
	 */
	maxnranges = 4096;
	nranges = 0;
	ranges = (myrange_t *) malloc(maxnranges * sizeof (myrange_t));

	/*
	 * Some info is populated lazily.
	 * So call twice; check only second time.
	 * The need to call twice might be a bug.
	 * So someday we should be able to eliminate the double call.
	 */
	dtrace_object_iter(h, mod_callback, (void *)0);
	dtrace_object_iter(h, mod_callback, (void *)1);

	dtrace_close(h);

	printf("%d preliminary errors\n", nerrors);

	/*
	 * Sort the ranges from among all modules and check for overlaps.
	 */
	qsort(ranges, nranges, sizeof (myrange_t), &mycompare);
	for (i = 1; i < nranges; i++) {
		if ( ranges[i-1].addr + ranges[i-1].size
		    > ranges[i].addr ) {
			printf("ERROR:");
			print_range(&ranges[i-1]);
			printf(" overlaps");
			print_range(&ranges[i]);
			printf("\n");
			nerrors++;
		}
	}

	printf("test tst.merge_ranges_bug25767469.c had %d errors\n", nerrors);
	return (nerrors != 0);
}

