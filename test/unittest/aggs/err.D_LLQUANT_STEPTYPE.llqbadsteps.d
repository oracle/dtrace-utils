/*
 * Oracle Linux DTrace.
 * Copyright (c) 2013, 2017, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  llquantize() steps must be an integer constant.
 *
 * SECTION: Aggregations/Aggregations
 *
 */


BEGIN
{
	x = 'a';
	@a[1] = llquantize(timestamp, 10, 0, 6, x);
}

