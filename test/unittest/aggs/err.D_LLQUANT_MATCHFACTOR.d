/*
 * Oracle Linux DTrace.
 * Copyright (c) 2013, 2017, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 *  llquantize() factor must match in every use of the aggregation.
 *
 * SECTION: Aggregations/Aggregations
 *
 */


BEGIN
{
	@ = llquantize(0, 10, 0, 6, 20);
	@ = llquantize(0, 20, 0, 6, 20);
}

