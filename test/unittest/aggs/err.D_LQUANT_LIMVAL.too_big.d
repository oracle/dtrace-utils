/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: lquantize() limit must be a signed 32-bit integer constant
 *
 * SECTION: Aggregations/Aggregations
 */

BEGIN
{
	@ = lquantize(1, 100, 2147483657);
	exit(0);
}
