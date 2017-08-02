/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *
 * Clause local variables are not initialized to zero.
 *
 * SECTION: Variables/Associative Arrays
 *
 *
 */

#pragma D option quiet

this int x;

BEGIN
{
	printf("the value of x is %d\n", this->x);
	exit(0);
}
