/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Simple profile provider test.
 * 	print the 'probeprov, probemod, probefunc, probename' at once.
 *	Match expected output in tst.probattrs.d.out
 *
 *
 * SECTION: profile Provider/tick-n probes;
 * 	Variables/Built-in Variables
 *
 */

#pragma D option quiet

BEGIN
{
	i = 0;
}

profile:::tick-1sec
{
	printf("%s %s %s %s", probeprov, probemod, probefunc, probename);
	exit (0);
}
