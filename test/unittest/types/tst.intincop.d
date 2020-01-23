/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 *	Verify increment operator using integers
 *
 * SECTION: Type and Constant Definitions/Enumerations
 *
 */

#pragma D option quiet


BEGIN
{
	int_orig = 100;
	int_pos = 100+1;
	int_neg = 100-1;

	int_pos_before = ++int_orig;
	int_orig = 100;
	int_neg_before = --int_orig;
	int_orig = 100;
	int_pos_after = int_orig++;
	int_orig = 100;
	int_neg_after = int_orig--;
	int_orig = 100;

}

tick-1
/int_pos_before  == int_pos && int_neg_before == int_neg &&
	int_pos_after == int_orig && int_pos_after == int_orig/
{
	exit(0);
}


tick-1
/int_pos_before  != int_pos || int_neg_before != int_neg ||
	int_pos_after != int_orig || int_pos_after != int_orig/
{
	exit(1);
}
