/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The trace() action prints a struct correctly.
 *
 * SECTION: Actions and Subroutines/trace()
 */

struct {
	int	a;
	char	b;
	int	c;
} st;

BEGIN
{
	st.a = 0x1234;
	st.b = 0;
	st.c = 0x9876;
	trace(st);
	exit(0);
}
