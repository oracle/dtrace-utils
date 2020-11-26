/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Simple Arithmetic expressions.
 *	Call simple expressions and make sure test succeeds.
 *	Match expected output in tst.basics.r
 *
 * SECTION: Types, Operators, and Expressions/Arithmetic Operators
 *
 */

#pragma D option quiet

BEGIN
{
	i = 0;
	i = 1 + 2 + 3;
	printf("The value of i is %d\n", i);

	i = i * 3;
	printf("The value of i is %d\n", i);

	i = (i * 3) + i;
	printf("The value of i is %d\n", i);

	i = (i + (i * 3) + i) * i;
	printf("The value of i is %d\n", i);

	i = i - (i + (i * 3) + i) * i / i * i;
	printf("The value of i is %d\n", i);

	i = i * (i - 3 + 5 / i * i ) / i * 6;
	printf("The value of i is %d\n", i);

	i = i ^ 5;
	printf("The value of i is %d\n", i);

	i = 0xdeadbeef; j = ~i; printf("~ %x is %x\n", i, j);
	i = 0x0000beef; j = ~i; printf("~ %x is %x\n", i, j);

	x = 0xdeadbeefdeadbeefull; y = ~x; printf("~ %x is %x\n", x, y);
	x = 0x00000000deadbeefull; y = ~x; printf("~ %x is %x\n", x, y);

	exit (0);
}
