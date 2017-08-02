/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2012, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Test assigning a variable two different incompatible types.  This should
 *  result in a compile-time error.
 *
 * SECTION: Variables/Associative Arrays
 *
 */

BEGIN
{
	x[123] = *`cad_pid;
	x[456] = `max_pfn;
}
