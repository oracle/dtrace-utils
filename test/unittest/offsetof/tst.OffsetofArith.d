/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/*
 * ASSERTION: offsetof can be used anywhere in a D program that an integer
 * constant can be used.
 *
 * SECTION: Structs and Unions/Member Sizes and Offsets
 *
 * NOTES:
 *
 */

#pragma D option quiet

typedef struct record {
	char c;
	int x;
	int y;
} record_t;

BEGIN
{

	add = offsetof(record_t, c) + offsetof(record_t, x) +
	    offsetof(record_t, y);
	sub = offsetof(record_t, y) - offsetof(record_t, x);
	mul = offsetof(record_t, x) * offsetof(record_t, c);
	div = offsetof(record_t, y) / offsetof(record_t, x);

	printf("offsetof(record_t, c) = %d\n", offsetof(record_t, c));
	printf("offsetof(record_t, x) = %d\n", offsetof(record_t, x));
	printf("offsetof(record_t, y) = %d\n", offsetof(record_t, y));

	printf("Addition of offsets (c+x+y)= %d\n", add);
	printf("Subtraction of offsets (y-x)= %d\n", sub);
	printf("Multiplication of offsets (x*c) = %d\n", mul);
	printf("Division of offsets (y/x) = %d\n", div);

	exit(0);
}

END
/(8 != offsetof(record_t, y)) || (4 != offsetof(record_t, x)) ||
    (0 != offsetof(record_t, c)) || (12 != add)  || (4 != sub) || (0 != mul)
    || (2 != div)/
{
	exit(1);
}
