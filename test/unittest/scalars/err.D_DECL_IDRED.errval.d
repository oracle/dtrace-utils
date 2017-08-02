/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Declare a variable with different data types.
 *
 * SECTION:  Variables/Scalar Variables
 *
 */
int x;
char x;
double x;
long x;

BEGIN
{
	x = 123;
}
