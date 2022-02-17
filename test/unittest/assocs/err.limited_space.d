/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 - no drops yet */

/*
 * ASSERTION: Trying to store more associative array elements than we have
 *	      space for will trigger a dynamic variable drop.
 *
 * SECTION: Variables/Thread-Local Variables
 */

#pragma D option dynvarsize=15

BEGIN
{
	a[1] = 1;
	a[2] = 2;
	a[3] = 3;
	a[4] = 4;
}

BEGIN
{
	exit(0);
}

ERROR
{
	exit(1);
}
