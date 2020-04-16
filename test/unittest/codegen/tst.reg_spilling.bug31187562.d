/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Test that the code generator will spill registers to the stack
 *	      and restore them in order to make them available for allocation.
 */

BEGIN
{
	a = 1000;
	b = 200;
	c = 30;
	d = 4;
	trace(a++ + (b++ + (c++ + d++)));
	exit(0);
}
