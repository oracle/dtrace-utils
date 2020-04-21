/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *
 * Declare a this variable and assign value of self variable to it.
 *
 * SECTION:  Variables/Scalar Variables
 *
 */
self int x;
this int y;

BEGIN
{
	this->x = 123;
	self->y = this->x;
	exit (0);
}
