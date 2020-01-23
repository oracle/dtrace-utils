/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 * Integer arithmetic can be performed on pointers by casting to uintptr_t.
 *
 * SECTION: Pointers and Arrays/Generic Pointers
 *
 * NOTES:
 *
 */

#pragma D option quiet

int array[3];
uintptr_t uptr;
int *p;
int *q;
int *r;

BEGIN
{
	array[0] = 20;
	array[1] = 40;
	array[2] = 80;

	uptr = (uintptr_t) &array[0];

	p = (int *) (uptr);
	q = (int *) (uptr + 4);
	r = (int *) (uptr + 8);

	printf("array[0]: %d\t*p: %d\n", array[0], *p);
	printf("array[1]: %d\t*q: %d\n", array[1], *q);
	printf("array[2]: %d\t*r: %d\n", array[2], *r);

	exit(0);
}

END
/(20 != *p) || (40 != *q) || (80 != *r)/
{
	printf("Error");
	exit(1);
}

