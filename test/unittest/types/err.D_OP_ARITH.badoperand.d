/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *   Arithmetic operations with a +/- must be followed by an arithmetic
 *	type.
 *
 * SECTION: Types, Operators, and Expressions/Arithmetic Operators
 *
 */



BEGIN
{
	p = 123 + -trace(0);
	exit(1);
}
