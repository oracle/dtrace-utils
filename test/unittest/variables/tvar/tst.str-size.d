/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Enough space is allocated for each string variable.
 *
 * SECTION: Variables/Scalar Variables
 */

#pragma D option quiet
#pragma D option strsize=4

BEGIN
{
	self->x = "abcd";
	self->y = "abcd";
	self->z = "abcd";
	trace(self->x);
	trace(self->y);
	trace(self->z);
	exit(0);
}

ERROR
{
	exit(1);
}
