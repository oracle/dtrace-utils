/*
 * Oracle Linux DTrace.
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Assignments to out-of-bound array loci are invalid.
 *
 * SECTION: Pointers and Arrays/Array Declarations and Storage
 */

int a[5];

BEGIN
{
	a[5] = 0;
	exit(0);
}
