/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: If dynvarsize is too small to hold a single dynamic variable,
 *	      report an error.
 *
 * SECTION: Aggregations/Aggregations
 */

/*
 * Storing an int (4 byte integer) in an associative array indexed by an int
 * requires at least 20 bytes of dynamic variable storage.
 */
#pragma D option dynvarsize=15

BEGIN
{
	a[1] = 1;
	exit(0);
}

ERROR
{
	exit(1);
}
