/*
 * Oracle Linux DTrace.
 * Copyright (c) 2013, 2017 Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 *  llquantize() steps must evenly factor*factor when steps>factor and lmag==0.
 *
 * SECTION: Aggregations/Aggregations
 *
 */


BEGIN
{
	@a[1] = llquantize(timestamp, 10, 0, 6, 30);
}

