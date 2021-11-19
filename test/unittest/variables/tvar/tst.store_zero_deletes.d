/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Storing 0 into a TLS variable removes it from storage, making
 *	      room for another variable.
 *
 * SECTION: Variables/Thread-Local Variables
 */

#pragma D option quiet
#pragma D option dynvarsize=15

BEGIN
{
	self->a = 1;
	self->b = 2;
	self->c = 3;
	self->a = 0;
	self->d = 4;
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
