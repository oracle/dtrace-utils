/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The substr() subroutine supports positive index values passed by
 *	      variable.
 *
 * SECTION: Actions and Subroutines/substr()
 */

#pragma D option quiet

BEGIN
{
	idx = 0;
	printf("\n% 3d %s", idx, substr("abcdefghijklmnop", idx));
	idx = 1;
	printf("\n% 3d %s", idx, substr("abcdefghijklmnop", idx));
	idx = 2;
	printf("\n% 3d %s", idx, substr("abcdefghijklmnop", idx));
	idx = 3;
	printf("\n% 3d %s", idx, substr("abcdefghijklmnop", idx));
	idx = 4;
	printf("\n% 3d %s", idx, substr("abcdefghijklmnop", idx));
	idx = 5;
	printf("\n% 3d %s", idx, substr("abcdefghijklmnop", idx));
	idx = 6;
	printf("\n% 3d %s", idx, substr("abcdefghijklmnop", idx));
	idx = 7;
	printf("\n% 3d %s", idx, substr("abcdefghijklmnop", idx));
	idx = 8;
	printf("\n% 3d %s", idx, substr("abcdefghijklmnop", idx));
	idx = 9;
	printf("\n% 3d %s", idx, substr("abcdefghijklmnop", idx));
	idx = 10;
	printf("\n% 3d %s", idx, substr("abcdefghijklmnop", idx));
	exit(0);
}
