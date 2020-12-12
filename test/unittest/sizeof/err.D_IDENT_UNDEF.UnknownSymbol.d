/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The D compiler throws a D_IDENT_UNDEF error when sizeof is passed
 * an unknown symbol.
 *
 * SECTION: Structs and Unions/Member Sizes and Offsets
 *
 */
#pragma D option quiet

BEGIN
{
	printf("sizeof(`): %d\n", sizeof(`));
	exit(0);
}
