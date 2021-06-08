/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The substr() subroutine supports negative count values passed by
 *	      variable.
 *
 * SECTION: Actions and Subroutines/substr()
 */

#pragma D option quiet

BEGIN
{
	cnt = -10;
	printf("\n% 3d % 3d %s", 2, cnt, substr("abcdefghijklmnop", 2, cnt));
	cnt = -9;
	printf("\n% 3d % 3d %s", 2, cnt, substr("abcdefghijklmnop", 2, cnt));
	cnt = -8;
	printf("\n% 3d % 3d %s", 2, cnt, substr("abcdefghijklmnop", 2, cnt));
	cnt = -7;
	printf("\n% 3d % 3d %s", 2, cnt, substr("abcdefghijklmnop", 2, cnt));
	cnt = -6;
	printf("\n% 3d % 3d %s", 2, cnt, substr("abcdefghijklmnop", 2, cnt));
	cnt = -5;
	printf("\n% 3d % 3d %s", 2, cnt, substr("abcdefghijklmnop", 2, cnt));
	cnt = -4;
	printf("\n% 3d % 3d %s", 2, cnt, substr("abcdefghijklmnop", 2, cnt));
	cnt = -3;
	printf("\n% 3d % 3d %s", 2, cnt, substr("abcdefghijklmnop", 2, cnt));
	cnt = -2;
	printf("\n% 3d % 3d %s", 2, cnt, substr("abcdefghijklmnop", 2, cnt));
	cnt = -1;
	printf("\n% 3d % 3d %s", 2, cnt, substr("abcdefghijklmnop", 2, cnt));
	exit(0);
}
