/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: An aggregation may not be used as a multi-dimensional array
 *
 * SECTION: Aggregations/Aggregations
 */

BEGIN
{
	@[0][1] = count();
	exit(0);
}
