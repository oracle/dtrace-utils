/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */


/*
 * ASSERTION:
 *	-> can only be used with pointer operands.
 *
 * SECTION: Pointers and Arrays/Pointers and Address Spaces
 */

#pragma D option quiet

struct teststruct {
	int a;
	int b;
};

struct teststruct xyz;

BEGIN
{
	xyz->a = 1;
	exit(1);
}
