/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	collect ppid at every fbt probe and at every firing of a
 *	high-frequency profile probe
 */

fbt:::
{
	@a[ppid] = count();
}

profile-4999hz
{
	@a[ppid] = count();
}

tick-1sec
/n++ == 10/
{
	exit(0);
}
