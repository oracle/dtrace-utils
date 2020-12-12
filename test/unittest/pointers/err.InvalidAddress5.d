/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Using integer arithmetic providing a non-aligned memory address will throw
 * a runtime error.
 *
 * SECTION: Pointers and Arrays/Generic Pointers
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

	uptr = (uintptr_t)&array[0];

	p = (int *)(uptr);
	q = (int *)(uptr + 2);
	r = (int *)(uptr + 3);

	printf("array[0]: %d\t*p: %d\n", array[0], *p);
	printf("array[1]: %d\t*q: %d\n", array[1], *q);
	printf("array[2]: %d\t*r: %d\n", array[2], *r);

	exit(0);
}

ERROR
{
	exit(1);
}
