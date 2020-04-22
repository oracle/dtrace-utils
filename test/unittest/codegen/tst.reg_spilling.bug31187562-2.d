/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Test that the code generator will spill registers to the stack
 *	      and restore them in order to make them available for allocation.
 *	      Test some other stack variables for corruption.
 */

BEGIN
{
	this->x = this->y = this-> z = 123456789;
	a = b = c = d = e = f = g = h = i = j = k = 87654321;

	/* cause register spills */
	trace(a++ & (b++ & (c++ & (d++ & (e++ & (f++ & (g++ & (h++ & (i++ & (j++ & k++))))))))));
	trace(a++ & (b++ & (c++ & (d++ & (e++ & (f++ & (g++ & (h++ & (i++ & (j++ & k++))))))))));
	trace(a++ & (b++ & (c++ & (d++ & (e++ & (f++ & (g++ & (h++ & (i++ & (j++ & k++))))))))));

	/* check local variables (on the stack near spilled registers)  */
	trace(this->x);
	trace(this->y);
	trace(this->z);

	exit(0);
}
