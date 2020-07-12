/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Verify relational operators with integers
 *
 * SECTION: Types, Operators, and Expressions/Relational Operators;
 * 	Types, Operators, and Expressions/Precedence
 *
 */

#pragma D option quiet


BEGIN
{
	int_1 = 0x100;
	int_2 = 0x101;
	int_3 = 0x99;
}

tick-1
/int_1 >= int_2 || int_2 <= int_1 || int_1 == int_2/
{
	printf("Shouldn't end up here (1)\n");
	printf("int_1 = %x int_2 = %x int_3 = %x\n",
		(int) int_1, (int) int_2, (int) int_3);
	exit(1);
}

tick-1
/int_3 > int_1 || int_1 < int_3 || int_3 == int_1/
{
	printf("Shouldn't end up here (2)\n");
	printf("int_1 = %x int_2 = %x int_3 = %x\n",
		(int) int_1, (int) int_2, (int) int_3);
	exit(1);
}

tick-1
/int_2 > int_3 && int_1 < int_2 ^^ int_3 == int_2 && !(int_1 != int_2)/
{
	exit(0);
}
