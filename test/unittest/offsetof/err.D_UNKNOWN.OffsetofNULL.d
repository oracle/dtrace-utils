/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Querying the offsetof an non-member variable of a struct throws
 * a D_UNKNOWN error.
 *
 * SECTION: Structs and Unions/Member Sizes and Offsets
 *
 */
#pragma D option quiet

struct record {
	int a;
	int b;
	int c : 4;
};

BEGIN
{
	printf("offsetof (struct record, NULL): %d\n",
	offsetof (struct record, NULL));
	exit(0);
}
