/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Assignment operators do not supported on all types
 *
 * SECTION: Types, Operators, and Expressions/Assignment Operators
 *
 */

#pragma D option quiet

BEGIN
{
	a = 3;
	a += "foobar";
	exit(1);
}
