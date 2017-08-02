/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2012, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Verify increment/decrement operator using pointers
 *
 * SECTION: Types, Operators, and Expressions/Increment and Decrement Operators
 *
 */

#pragma D option quiet


BEGIN
{
	ptr_orig = &`max_pfn;
	ptr_pos = &`max_pfn+1;
	ptr_neg = &`max_pfn-1;

	ptr_pos_before = ++ptr_orig;
	ptr_orig = &`max_pfn;
	ptr_neg_before = --ptr_orig;

	ptr_orig = &`max_pfn;
	ptr_pos_after = ptr_orig++;
	ptr_orig = &`max_pfn;
	ptr_neg_after = ptr_orig--;
	ptr_orig = &`max_pfn;

}

tick-1
/ptr_pos_before  == ptr_pos && ptr_neg_before == ptr_neg &&
	ptr_pos_after == ptr_orig && ptr_pos_after == ptr_orig/
{
	exit(0);
}


tick-1
/ptr_pos_before  != ptr_pos || ptr_neg_before != ptr_neg ||
	ptr_pos_after != ptr_orig || ptr_pos_after != ptr_orig/
{
	exit(1);
}

