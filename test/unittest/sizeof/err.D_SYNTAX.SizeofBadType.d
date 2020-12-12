/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: sizeof returns the size in bytes of any D expression or data
 * type. When an operator or non-symbol is passed as an argument to sizeof
 * operator, the D_SYNTAX error is thrown by the compiler.
 *
 * SECTION: Structs and Unions/Member Sizes and Offsets
 */
#pragma D option quiet

BEGIN
{
	printf("sizeof(*): %d\n", sizeof(*));
	exit(0);
}
