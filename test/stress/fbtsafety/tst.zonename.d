/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * @@skip: Linux does not have zones
 */

/*
 * ASSERTION:
 * 	collect zonename at every fbt probe and at every firing of a
 *	high-frequency profile probe
 */

fbt:::
{
	@a[zonename] = count();
}

profile-4999hz
{
	@a[zonename] = count();
}

tick-1sec
/n++ == 10/
{
	exit(0);
}
