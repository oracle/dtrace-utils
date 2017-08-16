/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Call invalid / impossible arithmetic operations and make sure
 *	Test gives compilation error.
 *
 * SECTION: Types, Operators, and Expressions/Arithmetic Operators
 *
 */

#pragma D option quiet

BEGIN
{
	i = 1;
	i /-= i;
	printf("The value of i is %d\n", i);
	exit (0);
}
