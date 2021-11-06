/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The substr() subroutine supports positive index values passed as
 *	      constants.
 *
 * SECTION: Actions and Subroutines/substr()
 */

#pragma D option quiet

BEGIN
{
	printf("\n% 3d %s",   0, substr("abcdefghijklmnop", 0));
	printf("\n% 3d %s",   1, substr("abcdefghijklmnop", 1));
	printf("\n% 3d %s",   2, substr("abcdefghijklmnop", 2));
	printf("\n% 3d %s",   3, substr("abcdefghijklmnop", 3));
	printf("\n% 3d %s",   4, substr("abcdefghijklmnop", 4));
	printf("\n% 3d %s",   5, substr("abcdefghijklmnop", 5));
	printf("\n% 3d %s",   6, substr("abcdefghijklmnop", 6));
	printf("\n% 3d %s",   7, substr("abcdefghijklmnop", 7));
	printf("\n% 3d %s",   8, substr("abcdefghijklmnop", 8));
	printf("\n% 3d %s",   9, substr("abcdefghijklmnop", 9));
	printf("\n% 3d %s",  10, substr("abcdefghijklmnop", 10));
	exit(0);
}
