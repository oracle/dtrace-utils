/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Verify relational operators with enumerations
 *
 * SECTION: Types, Operators, and Expressions/Relational Operators
 *
 */

#pragma D option quiet

enum numbers_1 {
	zero,
	one,
	two
};

enum numbers_2 {
	null,
	first,
	second
};

tick-1
/zero >= one || second <= first || zero == second/
{
	printf("Shouldn't end up here (1)\n");
	printf("zero = %d; one = %d; two = %d", zero, one, two);
	printf("null = %d; first = %d; second = %d", null, first, second);
	exit(1);
}

tick-1
/second < one || two > second || null == first/
{
	printf("Shouldn't end up here (2)\n");
	printf("zero = %d; one = %d; two = %d", zero, one, two);
	printf("null = %d; first = %d; second = %d", null, first, second);
	exit(1);
}

tick-1
/first < two && second > one && one != two && zero != first/
{
	exit(0);
}
