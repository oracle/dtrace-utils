/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Test the ternary operator.  Test left-hand side true, right-hand side true,
 *  and multiple nested instances of the ternary operator.
 *
 * SECTION:  Types, Operators, and Expressions/Conditional Expressions
 */

#pragma D option quiet

BEGIN
{
	x = 0;
	printf("x is %s\n", x == 0 ? "zero" : "one");
	x = 1;
	printf("x is %s\n", x == 0 ? "zero" : "one");
	x = 2;
	printf("x is %s\n", x == 0 ? "zero" : x == 1 ? "one" : "two");
	exit(0);
}
