/*
 * Oracle Linux DTrace.
 * Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Values that exceed 32 bits don't overflow.
 *
 * SECTION: Aggregations/Aggregations
 *
 */

#pragma D option quiet

BEGIN
{
	@a = llquantize(1100000000, 10, 7, 10, 10);
	@b = llquantize(1200000000, 10, 7, 10, 10);
	@c = llquantize(1300000000, 10, 7, 10, 10);
	@d = llquantize(1400000000, 10, 7, 10, 10);
	@e = llquantize(1788008280, 10, 7, 10, 10);
	exit(0);
}
