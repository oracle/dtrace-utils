/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Cannot dereference a void * pointer.
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

	printf("array[0]: %d, p: %d\n", array[0], *p);
	exit(0);
}
