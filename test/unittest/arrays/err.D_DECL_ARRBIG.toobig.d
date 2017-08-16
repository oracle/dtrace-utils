/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */


/*
 * ASSERTION:
 * 	Array declarations with indexs over INT_MAX return a
 *	D_DECL_ARRBIG errtag.
 *
 * SECTION:
 * 	Pointers and Arrays/Array Declarations and Storage
 */

int x[88294967295];

BEGIN
{
	exit(1);
}

