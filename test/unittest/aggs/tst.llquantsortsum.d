/*
 * Oracle Linux DTrace.
 * Copyright (c) 2013, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 *  llquantize() output is sorted correctly by the sum of the values.
 *
 * SECTION: Aggregations/Aggregations
 *
 */

#pragma D option quiet

BEGIN
{
	@["Larry"] = llquantize(1, 10, 0, 2, 20, -25);
	@["Moe"] = llquantize(2, 10, 0, 2, 20, -50);
	@["Shemp"] = llquantize(3, 10, 0, 2, 20, 50);
	@["Curly"] = llquantize(4, 10, 0, 2, 20, 25);

	printa(@);
	exit(0);
}
