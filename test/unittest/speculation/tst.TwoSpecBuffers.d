/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Increasing the value of nspec to two should will increase the number of
 * speculative buffers to two.
 *
 * SECTION: Speculative Tracing/Options and Tuning;
 *		Options and Tunables/nspec
 *
 */

#pragma D option quiet
#pragma D option cleanrate=3000hz
#pragma D option nspec=2

BEGIN
{
	var1 = 0;
	var2 = 0;
	var3 = 0;
}

BEGIN
{
	var1 = speculation();
	printf("Speculation ID: %d\n", var1);
	var2 = speculation();
	printf("Speculation ID: %d\n", var2);
	var3 = speculation();
	printf("Speculation ID: %d\n", var3);
}

BEGIN
/var1 && var2 && (!var3)/
{
	printf("Succesfully got two speculative buffers");
	exit(0);
}

BEGIN
/(!var1) || (!var2) || var3/
{
	printf("Test failed");
	exit(1);
}

ERROR
{
	exit(1);
}
