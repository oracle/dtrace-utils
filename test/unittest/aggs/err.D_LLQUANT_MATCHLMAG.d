/*
 * Oracle Linux DTrace.
 * Copyright (c) 2013, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: llquantize() low magnitude must match in every use of a given
 *	aggregation.
 *
 * SECTION: Aggregations/Aggregations
 */

BEGIN
{
	@ = llquantize(0, 10, 0, 6, 20);
	@ = llquantize(0, 10, 1, 6, 20);
	exit(0);
}
