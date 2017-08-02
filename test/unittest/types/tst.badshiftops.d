/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Negative values and values greater than the LH operand are
 * 	accepted.
 *
 * SECTION: Types, Operators, and Expressions/Bitwise Operators
 */

#pragma D option quiet


BEGIN
{
	int_1 = 0xffff;

	nint = int_1 << -6;
	printf("%x %x\n", int_1, nint);
	nint = int_1 << 100;
	printf("%x %x\n", int_1, nint);
	exit(0);
}
