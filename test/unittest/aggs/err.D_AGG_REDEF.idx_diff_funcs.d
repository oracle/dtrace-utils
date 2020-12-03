/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Elements in an indexed aggregation must have same definitions.
 *
 * SECTION: Aggregations/Aggregations
 */

BEGIN
{
	@[1] = count();
	@[2] = sum(5);
	exit(0);
}
