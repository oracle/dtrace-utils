/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *   Invalid type name
 *
 * SECTION: Types, Operators, and Expressions/Data Types and Sizes
 *
 */


#pragma D option quiet

dtrace:::BEGIN
{
	i = (char int)0;
	exit(0);
}
