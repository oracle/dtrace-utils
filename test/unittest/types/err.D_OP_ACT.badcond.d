/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Tracing functions may not be second and third expressions
 *	of a conditional expr.
 *
 * SECTION: Types, Operators, and Expressions/Conditional Expressions
 *
 */

#pragma D option quiet


BEGIN
{
	i = 1;
	x = i == 0 ? trace(0): trace(1);
	exit(1);
}
