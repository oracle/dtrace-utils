/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Simple profile provider test;
 *	Call simple profile
 *	Match expected output in tst.profexit.d.out
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
	trace(i);
	exit(0);
}
