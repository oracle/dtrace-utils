/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	progenyof() should return non-zero if the pid passed is in the
 #	progeny of the calling process.
 *
 * SECTION: Actions and Subroutines/progenyof()
 *
 */

#pragma D option quiet


BEGIN
{
	res_1 = -1;
	res_2 = -1;
	res_3 = -1;

	res_1 = progenyof($ppid);	/* this will always be true */
	res_2  = progenyof($ppid + 1);  /* this will always be false */
	res_3 = progenyof(1);		/* this will always be true */
}


tick-1
/res_1 > 0 && res_2 == 0 && res_3 > 0/
{
	exit(0);
}

tick-1
/res_1 <= 0 || res_2 != 0 || res_3 <= 0/
{
	exit(1);
}
