/*
 * Oracle Linux DTrace.
 * Copyright (c) 2013, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: llquantize() factor must evenly divide steps when steps > factor
 *
 * SECTION: Aggregations/Aggregations
 */

BEGIN
{
	@ = llquantize(1, 10, 0, 6, 25);
	exit(0);
}
