/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Post-decrement should work for global associative arrays.
 *
 * SECTION: Variables/Associative Arrays
 */

#pragma D option quiet

BEGIN
{
	old = a["a"] = 42;
	val = a["a"]--;

	exit(val != old || a["a"] != old - 1);
}
