/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */


/*
 * ASSERTION:
 * 	Arrays must have array dimensions
 *
 * SECTION: Pointers and Arrays/Array Declarations and Storage
 */

int a[];

BEGIN
{
	exit(0);
}
