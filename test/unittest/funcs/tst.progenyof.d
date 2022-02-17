/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	progenyof() should return non-zero if the pid passed is in the
 *	progeny of the calling process.
 *
 * SECTION: Actions and Subroutines/progenyof()
 *
 */

/* @@trigger: pid-tst-float */

#pragma D option quiet

BEGIN
{
	/* exit status reports if any errors */
	exit(
		/* should be progeny of pid, ppid, and 1 */
		progenyof($pid) == 0 ||
		progenyof($ppid) == 0 ||
		progenyof(1) == 0 ||
		/* should not be progeny of pid+1 or trigger */
		progenyof($pid + 1) != 0 ||
		progenyof($target) != 0
	);
}
