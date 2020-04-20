/*
 * Oracle Linux DTrace.
 * Copyright (c) 2013, 2017, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 *  llquantize() factor must be at least 2.
 *
 * SECTION: Aggregations/Aggregations
 *
 */


BEGIN
{
	@a[1] = llquantize(timestamp, 1, 0, 6, 20);
}

