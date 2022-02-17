/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Default value of unassigned elements in asssociative string
 *	      arrays is 0.  This will cause an 'invalid address 0x0' fault
 *	      upon dereferencing.
 *
 * SECTION: Variables/Associative Arrays
 */

#pragma D option quiet

string s[int];

BEGIN
{
	trace(s[1]);
	exit(1);
}

ERROR
{
	exit(arg4 != 1 || arg5 != 0);
}
