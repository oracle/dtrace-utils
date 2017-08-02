/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/*
 * ASSERTION:
 * Test invocation of offsetof() with a struct type alias.
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
	printf("offsetof(record_t, c) = %d\n", offsetof(record_t, c));
	printf("offsetof(record_t, x) = %d\n", offsetof(record_t, x));
	printf("offsetof(record_t, y) = %d\n", offsetof(record_t, y));
	exit(0);
}

END
/(8 != offsetof(record_t, y)) || (4 != offsetof(record_t, x)) ||
    (0 != offsetof(record_t, c))/
{
	exit(1);
}
