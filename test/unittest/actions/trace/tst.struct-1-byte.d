/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The trace() action prints a struct { int8_t } correctly.
 *
 * SECTION: Actions and Subroutines/trace()
 */

struct {
	int8_t	x;
} st;

BEGIN
{
	st.x = 0x34;
	trace(st);
	exit(0);
}
