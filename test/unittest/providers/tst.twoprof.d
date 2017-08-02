/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Simple profile provider test.
 *	Call two different profile provider
 *	Match expected output in tst.twoprof.d.out
 *
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

profile:::tick-100msec
/i == 3/
{
	exit(0);
}
