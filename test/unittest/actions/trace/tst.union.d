/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The trace() action prints a union correctly.
 *
 * SECTION: Actions and Subroutines/trace()
 */

union {
	int	a;
	char	b[3];
	short	c;
} u;

BEGIN
{
	u.a = 0x12345678;
	trace(u);
	exit(0);
}
