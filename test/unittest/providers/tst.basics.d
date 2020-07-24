/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 - exit causes duplication of output */

/*
 * ASSERTION:
 * 	Simple profile provider test;
 *	Call same profile provider two times
 *	Match expected output in tst.basics.d.out
 *
 * SECTION: profile Provider/tick-n probes
 *
 */

#pragma D option quiet

BEGIN
{
	i = 0;
}

profile:::tick-1sec
/i < 3/
{
	i++;
	trace(i);
}

profile:::tick-1sec
/i == 3/
{
	exit(0);
}
