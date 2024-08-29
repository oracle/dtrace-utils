/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: This test reproduces the alignment error.
 *
 * SECTION: Type and Constant Definitions/Enumerations
 *
 * NOTES:
 *
 */

#pragma D option quiet

BEGIN
{
	x = (int *)64;
	y = *x;
	trace(y);
}

ERROR
{
	exit(1);
}
