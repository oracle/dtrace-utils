/*
 * Oracle Linux DTrace.
 * Copyright (c) 2013, 2017, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 *  llquantize() high magnitude must be a 16-bit unsigned integer.
 *
 * SECTION: Aggregations/Aggregations
 *
 */


BEGIN
{
	@a[1] = llquantize(timestamp, 10, 0, 65536, 20);
}

