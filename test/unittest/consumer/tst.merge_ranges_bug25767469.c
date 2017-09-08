/*
 * Oracle Linux DTrace.
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * 25767469 dtrace_update() does not merge address ranges properly
 * Verify fix.
 */

#include <stdio.h>
#include <dtrace.h>

int nerrors = 0;

int mod_callback(dtrace_hdl_t * h, const dtrace_objinfo_t * d, void * arg) {
	if (arg) {
		/*
		 * When arg!=0, check for a gap between consecutive ranges.
		 * The check ensures:
		 *   - ranges are in order
		 *   - ranges do not overlap or abut
		 */
		int i;

		/* dta_text_addrs */
		for (i = 1; i < d->dto_text_addrs_size; i++) {
			if ( d->dto_text_addrs[i-1].dar_va
			    + d->dto_text_addrs[i-1].dar_size
			    >= d->dto_text_addrs[i  ].dar_va ) {
				printf("ERROR: text range %lx-%lx"
				    " overlaps or abuts %lx-%lx\n",
				    d->dto_text_addrs[i-1].dar_va,
				    d->dto_text_addrs[i-1].dar_va
				    + d->dto_text_addrs[i-1].dar_size,
				    d->dto_text_addrs[i  ].dar_va,
				    d->dto_text_addrs[i  ].dar_va
				    + d->dto_text_addrs[i  ].dar_size);
				nerrors++;
			}
		}

		/* dto_data_addrs */
		for (i = 1; i < d->dto_data_addrs_size; i++) {
			if ( d->dto_data_addrs[i-1].dar_va
			    + d->dto_data_addrs[i-1].dar_size
			    >= d->dto_data_addrs[i  ].dar_va ) {
				printf("ERROR: data range %lx-%lx"
				    " overlaps or abuts %lx-%lx\n",
				    d->dto_data_addrs[i-1].dar_va,
				    d->dto_data_addrs[i-1].dar_va
				    + d->dto_data_addrs[i-1].dar_size,
				    d->dto_data_addrs[i  ].dar_va,
				    d->dto_data_addrs[i  ].dar_va
				    + d->dto_data_addrs[i  ].dar_size);
				nerrors++;
			}
		}
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
	int err;
	dtrace_hdl_t *h = dtrace_open(DTRACE_VERSION, 0, &err);
	if (h == NULL) {
		printf("ERROR: dtrace_open %d |%s|\n",
		    err, dtrace_errmsg(h, err));
		return 1;
	}

	/*
	 * Some info is populated lazily.
	 * So call twice; check only second time.
	 * The need to call twice might be a bug.
	 * So someday we should be able to eliminate the double call.
	 */
	dtrace_object_iter(h, mod_callback, (void *)0);
	dtrace_object_iter(h, mod_callback, (void *)1);

	dtrace_close(h);
	printf("test tst.merge_ranges_bug25767469.c had %d errors\n", nerrors);
	return (nerrors != 0);
}

