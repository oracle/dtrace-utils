/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: sizeof returns the size in bytes of any D expression or data
 * type. For sizeof strings the D compiler throws D_SIZEOF_TYPE.
 *
 * SECTION: Structs and Unions/Member Sizes and Offsets
 *
 */
#pragma D option quiet
#pragma D option strsize=256

BEGIN
{
	assoc_array["hello"] = "hello";
	assoc_array["hi"] = "hi";
	assoc_array["hello"] = "hello, world";

	printf("sizeof(assoc_array[\"hello\"]): %d\n",
	    sizeof(assoc_array["hello"]));
	printf("sizeof(assoc_array[\"hi\"]): %d\n",
	    sizeof(assoc_array["hi"]));

	exit(0);
}
