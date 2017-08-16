/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: sizeof returns the size in bytes of any D expression or data
 * type. For a string variable, the D compiler throws a D_SIZEOF_TYPE.
 *
 * SECTION: Structs and Unions/Member Sizes and Offsets
 *
 */
#pragma D option quiet
#pragma D option strsize=256

BEGIN
{
	var = "hello";
	printf("sizeof (var): %d\n", sizeof (var));
	exit(0);
}
