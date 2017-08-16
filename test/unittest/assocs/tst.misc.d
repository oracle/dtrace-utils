/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Test the different kinds of associative scalar references.
 *
 * SECTION: Variables/Associative Arrays
 *
 * NOTES:
 *  In particular, we test accessing a DTrace associative array
 *  defined with scalar type (first ref that forces creation as both global
 *  and TLS), and DTrace associative array scalar subsequent references
 *  (both global and TLS).
 *
 */

#pragma D option quiet

BEGIN
{
	i = 0;
}

tick-10ms
/i != 5/
{
	x[123, "foo"] = 123;
	self->x[456, "bar"] = 456;
	i++;
}

tick-10ms
/i != 5/
{
	printf("x[] = %d\n", x[123, "foo"]);
	printf("self->x[] = %d\n", self->x[456, "bar"]);
}

tick-10ms
/i == 5/
{
	exit(0);
}
