/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

/* cut string size back a little to ease pressure on BPF verifier */
#pragma D option strsize=184

BEGIN {
	x = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789@#abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789@#";
	y = "abcdefghijklmnopqrstuvwxyz";
	printf("  0 %3d\n", index(x, y));
	printf("  0 %3d\n", index(x, y,  -1));
	printf("  0 %3d\n", index(x, y,   0));
	printf(" 64 %3d\n", index(x, y,   1));
	printf(" -1 %3d\n", index(x, y, 100));
	printf(" -1 %3d\n", index(x, y, 200));

	y = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789@#a";
	printf("  0 %3d\n", index(x, y, -1));
	printf("  0 %3d\n", index(x, y));
	printf(" -1 %3d\n", index(x, y, 1));

	x = "";
	y = "klmnopqrstuvw";
	printf(" -1 %3d\n", index(x, y));
	printf("  0 %3d\n", index(y, x));

	x = "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz";
	y = "klmnopqrstuvw";
	printf(" 10 %3d\n", index(x, y, -1));
	printf(" 10 %3d\n", index(x, y));
	printf(" 10 %3d\n", index(x, y, 10));
	printf(" 36 %3d\n", index(x, y, 20));
	printf(" 36 %3d\n", index(x, y, 30));
	printf(" 62 %3d\n", index(x, y, 40));
	printf(" 62 %3d\n", index(x, y, 50));
	printf(" 62 %3d\n", index(x, y, 60));
	printf(" 88 %3d\n", index(x, y, 70));
	printf(" 88 %3d\n", index(x, y, 80));
	printf("114 %3d\n", index(x, y, 90));

	exit(0);
}
