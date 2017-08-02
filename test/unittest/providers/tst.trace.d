/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Simple profile provider test.
 *	Call simple profile with trace() in END profile
 *	Match expected output in tst.trace.d.out
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
{
	i++;
	exit(0);
}

dtrace:::END
{
	trace(i);
}
