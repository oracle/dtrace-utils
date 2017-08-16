/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: D pointers do not allow invalid pointer accesses.
 *
 * SECTION: Pointers and Arrays/Pointer Safety
 *
 * NOTES:
 *
 */

#pragma D option quiet

BEGIN
{
	x = (int *)alloca(sizeof (int));
	trace(x);
	y = (int *) (x - 3300778156056);
	*y = 3;
	trace(*y);
}

ERROR
{
	exit(1);
}
