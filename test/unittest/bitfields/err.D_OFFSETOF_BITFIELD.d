/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Cannot apply offsetof operator to a bit-field member.
 *
 * SECTION: Structs and Unions/Bit-Fields
 *
 */
#pragma D option quiet

struct bitRecord{
	int a : 17;
	int b : 3;
	int c : 12;
} var;

BEGIN
{
	printf("offsetof(struct bitRecord, a): %d\n",
	offsetof(struct bitRecord, a));
	printf("offsetof(struct bitRecord, b): %d\n",
	offsetof(struct bitRecord, b));
	printf("offsetof(struct bitRecord, c): %d\n",
	offsetof(struct bitRecord, c));

	exit(0);
}
