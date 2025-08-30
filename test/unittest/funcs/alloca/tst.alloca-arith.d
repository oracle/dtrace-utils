/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Subtracting alloca pointers yields a plain integer value.
 *
 * SECTION: Actions and Subroutines/alloca()
 */

#pragma D option quiet

BEGIN
{
	x = (char *)alloca(1);
	x = (char *)alloca(10);
	y = (char *)alloca(1);
	z = y - x;
	z /= 8;
	exit(z == 2 ? 0 : 1);
}

ERROR
{
	exit(1);
}
