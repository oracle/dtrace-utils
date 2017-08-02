/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Test invocation of offsetof() with a union type.
 *
 * SECTION: Structs and Unions/Member Sizes and Offsets
 *
 * NOTES:
 *
 */

#pragma D option quiet

union s {
	int x;
	int y;
};

BEGIN
{
	printf("offsetof(s, y) = %d\n", offsetof(union D`s, y));
	exit(0);
}
