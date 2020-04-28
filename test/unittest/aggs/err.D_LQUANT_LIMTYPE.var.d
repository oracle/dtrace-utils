/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: lquantize() limit must be an integer constant
 *
 * SECTION: Aggregations/Aggregations
 */

BEGIN
{
	limit = 10;
	@ = lquantize(1, 100, limit);
	exit(0);
}
