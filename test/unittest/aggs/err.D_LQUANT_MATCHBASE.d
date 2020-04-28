/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: lquantize() base must match in every use of a given aggregation.
 *
 * SECTION: Aggregations/Aggregations
 */

BEGIN
{
	@ = lquantize(0, 10, 20, 1);
	@ = lquantize(0, 15, 20, 1);
	exit(0);
}
