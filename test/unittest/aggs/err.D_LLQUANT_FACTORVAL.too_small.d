/*
 * Oracle Linux DTrace.
 * Copyright (c) 2013, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: llquantize() factor must be at least 2.
 *
 * SECTION: Aggregations/Aggregations
 */

BEGIN
{
	@ = llquantize(1, 1, 0, 6, 20);
	exit(0);
}
