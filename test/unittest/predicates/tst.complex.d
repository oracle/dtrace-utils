/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 * 	Complex operations and if,then test.
 *	Call 'n' permutation and combination of operations over if,then.
 *	Match expected output in tst.complex.d.out
 *
 * SECTION: Program Structure/Predicates
 *
 */

#pragma D option quiet

BEGIN
{
	i = 0;
	j = 0;
}

tick-10ms
/i < 10/
{
	i++;
	j++;
	printf("\n\n%d\n------\n", i);
}

tick-10ms
/i == 5 || i == 10/
{
	printf("i == 5 (or) i == 10\n");
}

tick-10ms
/i <= 5/
{
	printf("i <= 5\n");
}

tick-10ms
/j >= 5/
{
	printf("j >= 5\n");
}

tick-10ms
/j >= 5 || i <= 5/
{
	printf("i >= 5 || j >= 5\n");
}

tick-10ms
/j >= 5 && i <= 5/
{
	printf("j >= 5 && i <= 55\n");
}

tick-10ms
/i < 5/
{
	printf("i < 5\n");
}

tick-10ms
/i == 2 || j == 2/
{
	printf("i == 2 (or) j == 2\n");
}

tick-10ms
/i == 2 && j == 2/
{
	printf("i == 2 (and) j == 2\n");
}

tick-10ms
/j != 10/
{
	printf("j != 10\n");
}

tick-10ms
/j == 5 || i == 2/
{
	printf("j == 5 || i == 2\n");
}

tick-10ms
/j == 5 && i == 2/
{
	printf("j == 5 && i == 2\n");
}

tick-10ms
/i == 10/
{
	printf("i == 10\n");
	exit(0);
}
