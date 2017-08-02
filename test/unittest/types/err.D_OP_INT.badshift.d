/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Shift operators not supported on all types
 *
 * SECTION: Types, Operators, and Expressions/Bitwise Operators
 *
 */

#pragma D option quiet

BEGIN
{
	x = "char" << 2;
	exit(1);
}
