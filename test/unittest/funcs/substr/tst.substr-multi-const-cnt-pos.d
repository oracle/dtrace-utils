/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The substr() subroutine supports positive count values passed as
 *	      constants.
 *
 * SECTION: Actions and Subroutines/substr()
 */

#pragma D option quiet

BEGIN
{
	printf("\n% 3d % 3d %s", 2,   0, substr("abcdefghijklmnop", 2, 0));
	printf("\n% 3d % 3d %s", 2,   1, substr("abcdefghijklmnop", 2, 1));
	printf("\n% 3d % 3d %s", 2,   2, substr("abcdefghijklmnop", 2, 2));
	printf("\n% 3d % 3d %s", 2,   3, substr("abcdefghijklmnop", 2, 3));
	printf("\n% 3d % 3d %s", 2,   4, substr("abcdefghijklmnop", 2, 4));
	printf("\n% 3d % 3d %s", 2,   5, substr("abcdefghijklmnop", 2, 5));
	printf("\n% 3d % 3d %s", 2,   6, substr("abcdefghijklmnop", 2, 6));
	printf("\n% 3d % 3d %s", 2,   7, substr("abcdefghijklmnop", 2, 7));
	printf("\n% 3d % 3d %s", 2,   8, substr("abcdefghijklmnop", 2, 8));
	printf("\n% 3d % 3d %s", 2,   9, substr("abcdefghijklmnop", 2, 9));
	printf("\n% 3d % 3d %s", 2,  10, substr("abcdefghijklmnop", 2, 10));
	exit(0);
}
