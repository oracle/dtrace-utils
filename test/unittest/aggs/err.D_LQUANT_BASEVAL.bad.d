/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */


/*
 * ASSERTION:
 *  lquantize() lower bound must be a 32-bit quantity
 *
 * SECTION: Aggregations/Aggregations
 *
 */

BEGIN
{
	@a[1] = lquantize(timestamp, 2147483657, 1000, 1);
}

