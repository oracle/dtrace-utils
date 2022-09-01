/*
 * Oracle Linux DTrace.
 * Copyright (c) 2017, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@nosort */

/*
 * ASSERTION:
 *  llquantize() output is sorted correctly by:
 *    - the sum of the values, then
 *    - the "zero" value, then
 *    - the key
 *  This behavior is not documented in detail;
 *  we simply implement the Solaris behavior
 *  and test it accordingly.
 *
 *  Two cases are tested.
 *    - "normal"
 *    - "tricky" (lmag==0 and steps>factor)
 *  The latter exercises extra code paths.
 *
 *  The keys appear in the script in some scrambled order.
 *
 *  These older tests are presumably subsets of the present test coverage
 *  and can be removed:
 *    - tst.llquantsortkey.[d|r]
 *    - tst.llquantsortsum.[d|r]
 *    - tst.llquantsortzero.[d|r]
 *
 * SECTION: Aggregations/Aggregations
 *
 */

#pragma D option quiet

BEGIN
{
	@normal["r"] = llquantize( 2, 2, 0, 1, 2, 1);
	@normal["z"] = llquantize(-4, 2, 0, 1, 2, 1);
	@normal["v"] = llquantize( 0, 2, 0, 1, 2, 1);
	@normal["x"] = llquantize(-2, 2, 0, 1, 2, 1);
	@normal["q"] = llquantize( 3, 2, 0, 1, 2, 1);
	@normal["u"] = llquantize( 0, 2, 0, 1, 2, 2);
	@normal["y"] = llquantize(-3, 2, 0, 1, 2, 1);
	@normal["t"] = llquantize( 0, 2, 0, 1, 2, 2);
	@normal["p"] = llquantize( 4, 2, 0, 1, 2, 1);
	@normal["w"] = llquantize(-1, 2, 0, 1, 2, 1);
	@normal["s"] = llquantize( 1, 2, 0, 1, 2, 1);

	@tricky["r"] = llquantize( 2, 2, 0, 1, 4, 1);
	@tricky["z"] = llquantize(-4, 2, 0, 1, 4, 1);
	@tricky["v"] = llquantize( 0, 2, 0, 1, 4, 1);
	@tricky["x"] = llquantize(-2, 2, 0, 1, 4, 1);
	@tricky["q"] = llquantize( 3, 2, 0, 1, 4, 1);
	@tricky["u"] = llquantize( 0, 2, 0, 1, 4, 2);
	@tricky["y"] = llquantize(-3, 2, 0, 1, 4, 1);
	@tricky["t"] = llquantize( 0, 2, 0, 1, 4, 2);
	@tricky["p"] = llquantize( 4, 2, 0, 1, 4, 1);
	@tricky["w"] = llquantize(-1, 2, 0, 1, 4, 1);
	@tricky["s"] = llquantize( 1, 2, 0, 1, 4, 1);

	printf("normal\n");
	printa(@normal);
	printf("tricky\n");
	printa(@tricky);
	exit(0);
}
