/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * DTrace has an intrinsically broken integer model.  Internally, it
 * represents all integers in 64-bit registers -- importantly, it assumes
 * that integers are represented in 64 bits and do not frequently need
 * type casting.  Recent patches have fixed some of the problems, but
 * other issues will remain until code generation becomes much more
 * diligent about type casting.  This test is an XFAIL placeholder
 * illustrating a few problems until these issues are addressed.
 *
 * Specifically, when signed integers are represented as 64-bit sign-extended
 * values, results of their operations are sometimes improperly represented --
 * say, when the uppermost bit flips.  The uppermost bits need to refilled.
 *
 * Check with a similar C program, albeit with the format string "%llx".
 */
/* @@xfail: broken DTrace integer model */

#pragma D option quiet

int x, y;
long long z;

BEGIN
{
	/* if x=1<<30, then x+x fills the uppermost bit */
	x = (1 << 30 );
	z =            (x + x); printf("%x\n", z);
	z = (long long)(x + x); printf("%x\n", z);

	/* here is another way of examining the uppermost bits */
	z = 1;
	z <<= 32;
	printf("%x\n",             (x + x)  & z);
	printf("%x\n", ((long long)(x + x)) & z);

	/* if x=1, then x<<31 fills the uppermost bit */
	x = 1;
	z =            (x << 31); printf("%x\n", z);
	z = (long long)(x << 31); printf("%x\n", z);

	/* if x=1<<30, then x<<5 should zero out */
	x = 1 << 30;
	z =            (x << 5); printf("%x\n", z);
	z = (long long)(x << 5); printf("%x\n", z);

	/* if we flip the sign bit, the sign should change */
	x = +1;
	y = 1;
	z = (x ^ (y << 31));
	printf("flip sign bit of +1, result is %s\n",
	                  z > 0 ? "positive" : "negative");
	printf("flip sign bit of +1, result is %s\n",
	    (x ^ (y << 31)) > 0 ? "positive" : "negative");

	x = -1;
	y = 1;
	z = (x ^ (y << 31));
	printf("flip sign bit of -1, result is %s\n",
	                  z > 0 ? "positive" : "negative");
	printf("flip sign bit of -1, result is %s\n",
	    (x ^ (y << 31)) > 0 ? "positive" : "negative");

	exit (0);
}
