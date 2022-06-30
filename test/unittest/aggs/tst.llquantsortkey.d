/*
 * Oracle Linux DTrace.
 * Copyright (c) 2013, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  llquantize() output is sorted by key if the zeroth elements are equal and
 *  the sums are equal.
 *
 * SECTION: Aggregations/Aggregations
 *
 */

#pragma D option quiet

BEGIN
{
	@["Spanky"] = llquantize(10, 10, 0, 2, 20, 20);
	@["Buckwheat"] = llquantize(4, 10, 0, 2, 20, 50);
	@["Alfalfa"] = llquantize(2, 10, 0, 2, 20, 100);

	printa(@);
	exit(0);
}
