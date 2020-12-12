/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: sizeof returns the size in bytes of any D expression or data
 * type. For an associative array, the D compiler should throw an error since
 * an associative array does not have a fixed size.
 *
 * SECTION: Structs and Unions/Member Sizes and Offsets
 *
 */
#pragma D option quiet

BEGIN
{
	assoc_array[0] = 010;
	assoc_array[1] = 100;
	assoc_array[2] = 210;

	printf("sizeof(assoc_array): %d\n", sizeof(assoc_array));
	printf("sizeof(assoc_array[0]): %d\n", sizeof(assoc_array[0]));
	printf("sizeof(assoc_array[1]): %d\n", sizeof(assoc_array[1]));
	printf("sizeof(assoc_array[2]): %d\n", sizeof(assoc_array[2]));

	exit(0);
}
