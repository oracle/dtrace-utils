/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Trying to access the address of a user defined dynamic variable should throw
 * a D_ADDROF_VAR error.
 *
 * SECTION: Pointers and Arrays/Generic Pointers
 * SECTION: Pointers and Arrays/Pointers to Dtrace Objects
 */

#pragma D option quiet

int i;
int *p;

BEGIN
{
	i = 10;
	p = &i;

	printf("Integer: %d, Pointer: %d\n", i, *p);
	exit(0);
}
