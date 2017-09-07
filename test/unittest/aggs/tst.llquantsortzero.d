/*
 * Oracle Linux DTrace.
 * Copyright (c) 2013, 2017, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  llquantize() output is sorted by the zeroth element if the sums are equal.
 *
 * SECTION: Aggregations/Aggregations
 *
 */

#pragma D option quiet

BEGIN
{
	@["Alfalfa"] = llquantize(0, 10, 0, 2, 20, 25);
	@["Buckwheat"] = llquantize(0, 10, 0, 2, 20, -25);
	@["Alfalfa"] = llquantize(2, 10, 0, 2, 20, 100);
	@["Buckwheat"] = llquantize(4, 10, 0, 2, 20, 50);

	printa(@);
	exit(0);
}
