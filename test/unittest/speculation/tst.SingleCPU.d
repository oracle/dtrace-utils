/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Verify many speculations (single-CPU case).
 *
 * SECTION: Speculative Tracing
 */

#pragma D option quiet

/* This test should take only 10s, but is taking much longer than the 10s
   one would expect (48s here).  Boosting timeout temporarily.  */
/* @@timeout: 120 */

BEGIN
{
	n = 0;
}

/*
 * Each tick, n is incremented.  Which clause is used rotates modulo 4.
 *   0: get a specid
 *   1: speculate some output
 *   2: speculate some other output
 *   3: commit (sometimes) or discard (usually)
 */

tick-10ms
/ (n & 3) == 0 /
{
	i = speculation();
}

tick-10ms
/ (n & 3) == 1 /
{
	speculate(i);
	printf("%4d %4d", n, i);
}

tick-10ms
/ (n & 3) == 2 /
{
	speculate(i);
	printf("%4d hello world\n", n);
}

tick-10ms
/ (n & 3) == 3 && (n & 63) == 3 /
{
	commit(i);
}

tick-10ms
/ (n & 3) == 3 && (n & 63) != 3 /
{
	discard(i);
}

tick-10ms
/ n++ >= 1000 /
{
	exit(0);
}
