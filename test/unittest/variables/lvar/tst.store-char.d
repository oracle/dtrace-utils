/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Storing an integer to a 'char' local variable does not store
 *	      more than a single byte.
 *
 * SECTION: Variables/Scalar Variables
 */

#pragma D option quiet

this char a, b;

BEGIN
{
	this->b = 4;
	this->a = 5;

	trace(this->a);
	trace(this->b);

	exit(0);
}
