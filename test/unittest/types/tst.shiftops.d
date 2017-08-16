/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Verify shift operators
 *
 * SECTION: Types, Operators, and Expressions/Bitwise Operators;
 * 	Types, Operators, and Expressions/Precedence
 */

#pragma D option quiet


BEGIN
{
	int_1 = 0xffff;

	nint = (((((((((((int_1 << 2 >> 2) << 3 >> 3) << 4 >> 4) << 5 >> 5)
		<< 6 >> 6) << 7 >> 7) << 8 >>8) << 9 >> 9) << 10 >> 10)
		<< 11 >> 11) << 12 >> 12);

}

tick-1
/nint != int_1/
{
	printf("Unexpected error nint = %x, expected %x\n", nint, int_1);
	exit(1);
}

tick-1
/nint == int_1/
{
	exit(0);

}
