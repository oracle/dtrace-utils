/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Loading the value of unassigned thread-local variables does not
 *	      cause space to be allocated for them
 *
 * SECTION: Variables/Thread-Local Variables
 */

#pragma D option quiet
#pragma D option dynvarsize=12

self x, y, z, w;

BEGIN
{
	trace(self->x);
	trace(self->y);
	trace(self->z);
	self->w = 1234;
	trace(self->w);
	exit(0);
}

ERROR
{
	exit(1);
}
