/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Verify unary operator
 *
 * SECTION: Types, Operators, and Expressions/Bitwise Operators
 */

#pragma D option quiet


BEGIN
{
	int_1 = 0xffff;
	int_2 = 0;
	int_3 = 0x0f0f;

	printf("%x %x %x\n", int_1, int_2, int_3);
	printf("%x %x %x\n", ~int_1, ~int_2, ~int_3);
	printf("%x %x %x\n", ~~int_1, ~~int_2, ~~int_3);
	printf("%x %x %x\n", ~~~int_1, ~~~int_2, ~~~int_3);
	exit(0);
}
