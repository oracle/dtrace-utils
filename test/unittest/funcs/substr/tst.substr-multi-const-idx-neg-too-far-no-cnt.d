/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The substr() subroutine supports negative index values passed as
 *	      constants, with some values out of range.
 *
 * SECTION: Actions and Subroutines/substr()
 */

#pragma D option quiet

BEGIN
{
	printf("\n% 3d %s", -10, substr("abcde", -10));
	printf("\n% 3d %s",  -9, substr("abcde", -9));
	printf("\n% 3d %s",  -8, substr("abcde", -8));
	printf("\n% 3d %s",  -7, substr("abcde", -7));
	printf("\n% 3d %s",  -6, substr("abcde", -6));
	printf("\n% 3d %s",  -5, substr("abcde", -5));
	printf("\n% 3d %s",  -4, substr("abcde", -4));
	printf("\n% 3d %s",  -3, substr("abcde", -3));
	printf("\n% 3d %s",  -2, substr("abcde", -2));
	printf("\n% 3d %s",  -1, substr("abcde", -1));
	exit(0);
}
