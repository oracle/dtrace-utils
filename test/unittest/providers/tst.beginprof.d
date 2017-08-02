/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Simple profile provider test.
 * 	print the 'probeprov, probemod, probefunc, probename' from
 *	BEGIN
 *	Match the output from tst.beginprof.d.out
 *
 * SECTION: profile Provider/profile-n probes;
 *	Variables/Built-in Variables
 *
 */

#pragma D option quiet

BEGIN
{
	printf("%s %s %s %s", probeprov, probemod, probefunc, probename);
	exit (0);
}
