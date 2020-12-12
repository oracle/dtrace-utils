/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 * Can dereference a void * pointer only by casting it to another type.
 *
 * SECTION: Pointers and Arrays/Generic Pointers
 */

#pragma D option quiet

void *p;

int array[3];

BEGIN
{
	array[0] = 234;
	array[1] = 334;
	array[2] = 434;

	p = &array[0];
	newp = (int *)p;

	printf("array[0]: %d, newp: %d\n", array[0], *newp);
	exit(0);
}

END
/234 != *newp/
{
	exit(1);
}
