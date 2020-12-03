/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */
/*
 * ASSERTION: printa() requires aggregation arguments to have matching key
 *	signatures
 *
 * SECTION: Output Formatting/printa()
 */

BEGIN
{
	@a = count();
	@b[1] = count();
	printa(@a, @b);
	exit(0);
}
