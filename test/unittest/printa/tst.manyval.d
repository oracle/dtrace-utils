/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 *  Test the use of many aggregation results in the same format string.
 *
 * SECTION: Output Formatting/printa()
 *
 */

#pragma D option quiet

BEGIN
{
	@a = count();
	printa("%@u %@u %@u %@u %@u %@u %@u\n", @a);
	exit(0);
}
