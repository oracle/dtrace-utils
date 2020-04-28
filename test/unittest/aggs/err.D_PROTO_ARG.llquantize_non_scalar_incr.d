/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: llquantize() increment value cannot be non-scalar
 *
 * SECTION: Aggregations/Aggregations
 */

BEGIN
{
	@ = llquantize(1, 10, 0, 2, 20, "");
	exit(0);
}
