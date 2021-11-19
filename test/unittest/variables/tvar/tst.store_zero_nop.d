/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Storing 0 into a new TLS variable does not allocate space.
 *
 * SECTION: Variables/Thread-Local Variables
 */

#pragma D option quiet
#pragma D option dynvarsize=5

BEGIN
{
	self->a = 1;
	self->b = 0;
	self->c = 0;
	self->d = 0;
	trace(self->a);
	trace(self->b);
	trace(self->c);
	trace(self->d);
	exit(0);
}

ERROR
{
	exit(1);
}
