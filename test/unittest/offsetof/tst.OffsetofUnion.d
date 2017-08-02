/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/*
 * ASSERTION:
 * Test invocation of offsetof() with a union type alias.
 *
 * SECTION: Structs and Unions/Member Sizes and Offsets
 *
 * NOTES:
 *
 */

#pragma D option quiet

union record {
	int x;
	int y;
	char c;
};

BEGIN
{
	printf("offsetof(record, x) = %d\n", offsetof(union D`record, x));
	printf("offsetof(record, y) = %d\n", offsetof(union D`record, y));
	printf("offsetof(record, c) = %d\n", offsetof(union D`record, c));
	exit(0);
}

END
/(0 != offsetof(union D`record, y)) && (0 != offsetof(union D`record, x)) &&
    (0 != offsetof(union D`record, c))/
{
	exit(1);
}
