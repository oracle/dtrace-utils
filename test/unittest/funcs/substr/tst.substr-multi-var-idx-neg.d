/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The substr() subroutine supports negative index values passed by
 *	      variable.
 *
 * SECTION: Actions and Subroutines/substr()
 */

#pragma D option quiet

BEGIN
{
	idx = -10;
	printf("\n% 3d % 3d %s", idx, 3, substr("abcdefghijklmnop", idx, 3));
	idx = -9;
	printf("\n% 3d % 3d %s", idx, 3, substr("abcdefghijklmnop", idx, 3));
	idx = -8;
	printf("\n% 3d % 3d %s", idx, 3, substr("abcdefghijklmnop", idx, 3));
	idx = -7;
	printf("\n% 3d % 3d %s", idx, 3, substr("abcdefghijklmnop", idx, 3));
	idx = -6;
	printf("\n% 3d % 3d %s", idx, 3, substr("abcdefghijklmnop", idx, 3));
	idx = -5;
	printf("\n% 3d % 3d %s", idx, 3, substr("abcdefghijklmnop", idx, 3));
	idx = -4;
	printf("\n% 3d % 3d %s", idx, 3, substr("abcdefghijklmnop", idx, 3));
	idx = -3;
	printf("\n% 3d % 3d %s", idx, 3, substr("abcdefghijklmnop", idx, 3));
	idx = -2;
	printf("\n% 3d % 3d %s", idx, 3, substr("abcdefghijklmnop", idx, 3));
	idx = -1;
	printf("\n% 3d % 3d %s", idx, 3, substr("abcdefghijklmnop", idx, 3));
	exit(0);
}
