/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The strlen() subroutine returns the correct result.
 *
 * SECTION: Actions and Subroutines/strlen()
 */

/* @@runtest-opts: -C */

#pragma D option quiet

#define TEST(s)	printf("%60s %d\n", (s), strlen(s))

BEGIN
{
	TEST("1");
	TEST("12");
	TEST("123");
	TEST("12345");
	TEST("123456");
	TEST("1234567");
	TEST("12345678");
	TEST("123456789");
	TEST("1234567890");
	TEST("12345678901234567890");
	TEST("123456789012345678901234567890");
	TEST("1234567890123456789012345678901234567890");
	TEST("12345678901234567890123456789012345678901234567890");
	TEST("123456789012345678901234567890123456789012345678901234567890");
	exit(0);
}
