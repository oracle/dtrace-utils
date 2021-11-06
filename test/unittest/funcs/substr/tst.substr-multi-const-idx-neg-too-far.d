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
	printf("\n% 3d % 3d %s", -10, 3, substr("abcde", -10, 3));
	printf("\n% 3d % 3d %s",  -9, 3, substr("abcde", -9, 3));
	printf("\n% 3d % 3d %s",  -8, 3, substr("abcde", -8, 3));
	printf("\n% 3d % 3d %s",  -7, 3, substr("abcde", -7, 3));
	printf("\n% 3d % 3d %s",  -6, 3, substr("abcde", -6, 3));
	printf("\n% 3d % 3d %s",  -5, 3, substr("abcde", -5, 3));
	printf("\n% 3d % 3d %s",  -4, 3, substr("abcde", -4, 3));
	printf("\n% 3d % 3d %s",  -3, 3, substr("abcde", -3, 3));
	printf("\n% 3d % 3d %s",  -2, 3, substr("abcde", -2, 3));
	printf("\n% 3d % 3d %s",  -1, 3, substr("abcde", -1, 3));
	exit(0);
}
