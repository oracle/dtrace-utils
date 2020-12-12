/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION: test for off-by-one error in the format lookup code
 *
 * SECTION: Aggregations/Aggregations; Misc
 */

/*
 * A script from Jon Haslam that induced an off-by-one error in the
 * format lookup code.
 */
BEGIN
{
	start = timestamp;
	allocd = 0;
	numallocs = 0;
	numfrees = 0;
	numtids = 0;
}

syscall:::entry
{
	@sys[tid] = sum(tid);
}

END
{
	printf("%s, %s, %s, %d numtids", "hhh", "jjj", "ggg", numtids);
	printa(@sys);
}

tick-1sec
/n++ == 5/
{
	exit(0);
}
