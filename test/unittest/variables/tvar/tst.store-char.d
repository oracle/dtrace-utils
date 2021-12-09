/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Storing an integer to a 'char' thread-local variable does not
 *	      store more than a single byte.
 *
 * SECTION: Variables/Scalar Variables
 */

#pragma D option quiet
#pragma D option dynvarsize=1024

self char a, b;

BEGIN
{
	self->b = 4;
	self->a = 5;

	trace(self->a);
	trace(self->b);

	exit(0);
}
