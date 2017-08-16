/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	positive check conditional expressions
 *
 * SECTION: Types, Operators, and Expressions/Conditional Expressions
 *
 * NOTES: these tests are from the User's Guide
 */

#pragma D option quiet


BEGIN
{
	i = 0;
	x = i == 0 ? "zero" : "non-zero";

	c = 'd';
	hexval = (c >= '0' && c <= '9') ? c - '0':
		(c >= 'a' && c <= 'z') ? c + 10 - 'a' : c + 10 - 'A';
}

tick-1
/x == "zero" && hexval == 13/
{
	exit(0);
}

tick-1
/x != "zero" || hexval != 13/
{
	exit(1);
}
