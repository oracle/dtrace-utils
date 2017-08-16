/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */


/*
 * ASSERTION:
 *	Tuples can not be used in non-associative arrays.
 *
 * SECTION: Pointers and Arrays/Array Declarations and Storage
 */

int x[5];

BEGIN
{
	x[1, 2] = 1;
}
