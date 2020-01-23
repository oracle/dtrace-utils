/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 *	Verify assignment operators
 *
 * SECTION: Types, Operators, and Expressions/Assignment Operators
 *
 */

#pragma D option quiet


BEGIN
{
	int_1 = 0x100;
	int_2 = 0xf0f0;
	int_3 = 0x0f0f;

	intn = 0x1;

	intn += int_1;
	printf("%x\n", intn);
	intn -= int_1;
	printf("%x\n", intn);
	intn *= int_1;
	printf("%x\n", intn);
	intn /= int_1;
	printf("%x\n", intn);
	intn %= int_1;
	printf("%x\n", intn);
	printf("\n");

	intb = 0x0000;

	intb |= (int_2 | intb);
	printf("%x\n", intb);
	intb &= (int_2 | int_3);
	printf("%x\n", intb);
	intb ^= (int_2 | int_3);
	printf("%x\n", intb);
	intb |= ~(intb);
	printf("%x\n", intb);
	printf("\n");

	intb = int_2;

	printf("%x\n", intb);
	intb <<= 3;
	printf("%x\n", intb);
	intb >>= 3;
	printf("%x\n", intb);

	exit(0);

}
