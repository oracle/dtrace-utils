/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Simple mod and div expressions should work correctly,
 *	even for signed operations.
 *
 * SECTION: Types, Operators, and Expressions/Arithmetic Operators
 *
 */

#pragma D option quiet

BEGIN {
	x = 19; y = 5;
	trace(( x) / ( y)); trace (( x) % ( y));
	trace(( x) / (-y)); trace (( x) % (-y));
	trace((-x) / ( y)); trace ((-x) % ( y));
	trace((-x) / (-y)); trace ((-x) % (-y));
	exit(0)
}
