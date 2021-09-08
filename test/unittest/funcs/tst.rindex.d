/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * Start at the prescribed index and work backwards for the first match.
 * The default start is len(str)-len(substr).
 */

#pragma D option quiet

/* cut string size back a little to ease pressure on BPF verifier */
#pragma D option strsize=144

BEGIN {
	x = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789@#abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789@#";
	y = "abcdefghijklmnopqrstuvwxyz";
	printf(" 64 %3d\n", rindex(x, y));
	printf(" -1 %3d\n", rindex(x, y,  -1));
	printf("  0 %3d\n", rindex(x, y,   0));
	printf("  0 %3d\n", rindex(x, y,   1));
	printf(" 64 %3d\n", rindex(x, y,  70));
	printf(" 64 %3d\n", rindex(x, y, 200));

	y = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789@#a";
	printf(" -1 %3d\n", rindex(x, y, -1));
	printf("  0 %3d\n", rindex(x, y));
	printf("  0 %3d\n", rindex(x, y, 1));

	x = "";
	y = "klmnopqrstuvw";
	printf(" -1 %3d\n", rindex(x, y));
	printf(" 13 %3d\n", rindex(y, x));

	x = "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz";
	y = "klmnopqrstuvw";
	printf("114 %3d\n", rindex(x, y));
	printf(" -1 %3d\n", rindex(x, y,  -1));
	printf(" -1 %3d\n", rindex(x, y,   0));
	printf(" -1 %3d\n", rindex(x, y,   5));
	printf(" 10 %3d\n", rindex(x, y,  10));
	printf(" 10 %3d\n", rindex(x, y,  30));
	printf(" 36 %3d\n", rindex(x, y,  40));
	printf(" 36 %3d\n", rindex(x, y,  60));
	printf(" 62 %3d\n", rindex(x, y,  70));
	printf(" 62 %3d\n", rindex(x, y,  80));
	printf(" 88 %3d\n", rindex(x, y,  90));
	printf("114 %3d\n", rindex(x, y, 120));
	printf("114 %3d\n", rindex(x, y, 200));

	x = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
	y = "aaaa";
	printf(" 28 %3d\n", rindex(x, y));
	printf(" -1 %3d\n", rindex(y, x));

	exit(0);
}
