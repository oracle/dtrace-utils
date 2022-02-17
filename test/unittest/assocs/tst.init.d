/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Default value of unassigned elements in asssociative arrays is 0
 *
 * SECTION: Variables/Associative Arrays
 */

#pragma D option quiet

BEGIN
{
	a["a"] = 42;

	exit(a["b"] != 0);
}
