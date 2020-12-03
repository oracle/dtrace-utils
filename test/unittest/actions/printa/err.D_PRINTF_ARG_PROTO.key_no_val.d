/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: printa() does not allow un-indexed aggregation arguments if a key
 *	conversion is specified in the format string
 *
 * SECTION: Output Formatting/printa()
 */

BEGIN
{
	@ = count();
	printa("%d", @);
	exit(0);
}
