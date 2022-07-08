/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/*
 * ASSERTION: printa() requires aggregation arguments to have matching key
 *	signatures (matching key component types)
 *
 * SECTION: Output Formatting/printa()
 */

BEGIN
{
	@a[1, "", 2] = count();
	@b[1, 2, ""] = count();
	printa(@a, @b);
	exit(0);
}
