/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 * 	Simple profile provider test.
 * 	print the 'probemod' field i.e. Current probe description's module
 *	field.
 *	Match expected output in tst.probemod.d.out
 *
 * SECTION: profile Provider/tick-n probes;
 *	Variables/Built-in Variables
 *
 */

#pragma D option quiet

BEGIN
{
	i = 0;
}

profile:::tick-1sec
{
	printf("probe description module field = %s", probemod);
	exit (0);
}
