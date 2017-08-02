/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Trying to access the address of a user defined array variable should throw
 * a D_ADDROF_VAR error.
 *
 * SECTION: Pointers and Arrays/Pointers and Address Spaces
 */

#pragma D option quiet

int a[5];
int *p;

BEGIN
{
	p = &a;

	exit(0);
}
