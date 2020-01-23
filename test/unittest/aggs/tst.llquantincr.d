/*
 * Oracle Linux DTrace.
 * Copyright (c) 2013, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 *  Optional increment parameter to llquantize() functions correctly.
 *
 * SECTION: Aggregations/Aggregations
 *
 */

#pragma D option quiet

tick-1ms
{
	@ = llquantize(i++, 10, 0, 6, 20, 2);
}

tick-1ms
/i == 1500/
{
	exit(0);
}
