/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	collect execname at every fbt probe and at every firing of a
 *	high-frequency profile probe
 */

fbt:::
{
	@a[execname] = count();
}

profile-4999hz
{
	@a[execname] = count();
}

tick-1sec
/n++ == 10/
{
	exit(0);
}
