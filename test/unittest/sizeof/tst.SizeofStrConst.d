/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: sizeof returns the size in bytes of any D expression or data
 * type. When a raw string is passed to the sizeof operator, the compiler
 * throws a D_SYNTAX error.
 *
 * SECTION: Structs and Unions/Member Sizes and Offsets
 */
#pragma D option quiet

BEGIN
{
	printf("sizeof \"hi\": %d\n", sizeof ("hi"));
	exit(0);
}
