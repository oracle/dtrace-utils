/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Conditional expressions not supported on all types
 *
 * SECTION: Types, Operators, and Expressions/Conditional Expressions
 *
 */

#pragma D option quiet

BEGIN
{
	a = "boo";
	c = "boo" ? "should": "fail";
	exit(1);
}
