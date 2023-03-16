/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Storing 0 into an associate array element that does not exist yet
 *	      does not actually store anything.  Therefore, such stores will
 *	      not trigger a dynamic varibale drop if we are out of space.
 *
 * SECTION: Variables/Thread-Local Variables
 */
#pragma D option dynvarsize=20
#pragma D option quiet

BEGIN
{
	a[1] = 1;
	a[2] = 0;
	a[3] = 0;
	a[4] = 0;
	trace(a[1]);
	trace(a[2]);
	trace(a[3]);
	trace(a[4]);
	exit(0);
}

ERROR
{
	exit(1);
}
