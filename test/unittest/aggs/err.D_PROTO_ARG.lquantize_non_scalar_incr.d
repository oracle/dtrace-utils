/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: lquantize() increment value cannot be non-scalar
 *
 * SECTION: Aggregations/Aggregations
 */

BEGIN
{
	@ = lquantize(1, 0, 10, 2, "");
	exit(0);
}
