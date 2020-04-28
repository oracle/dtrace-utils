/*
 * Oracle Linux DTrace.
 * Copyright (c) 2017, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: llquantize() steps must evenly divide pow(factor,lmag+1) when
 *	steps>factor and lmag>0.
 *
 * SECTION: Aggregations/Aggregations
 */

BEGIN
{
	@ = llquantize(1, 10, 2, 6, 80);
	exit(0);
}
