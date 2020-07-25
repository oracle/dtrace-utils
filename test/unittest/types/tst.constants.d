/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	verify that integer constants can be written in decimal
 *      octal or hexadecimal
 *
 * SECTION: Types, Operators, and Expressions/Constants
 */

#pragma D option quiet


BEGIN
{
	decimal = 12345;
	octal = 012345;
	hexadecimal_1 = 0x12345;
	hexadecimal_2 = 0X12345;

	printf("%d %d %d %d", decimal, octal, hexadecimal_1, hexadecimal_2);
	printf("%d %o %x %x", decimal, octal, hexadecimal_1, hexadecimal_2);

	exit(0);
}

