/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Unused pragmas throw error D_PRAGMA_UNUSED
 *
 * SECTION: Errtags/D_PRAGMA_UNUSED
 *
 */

void func(int, int);

#pragma D option quiet
#pragma D attributes Stable/Stable/Common func;

BEGIN
{
	exit(0);
}

ERROR
{
	exit(0);
}
