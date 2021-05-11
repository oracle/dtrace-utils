/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Storing an integer to a 'char' global variable does not store
 *	      more than a single byte.
 *
 * SECTION: Variables/Scalar Variables
 */

#pragma D option quiet

char a, b;

BEGIN
{
	b = 4;
	a = 5;

	trace(a);
	trace(b);

	exit(0);
}
