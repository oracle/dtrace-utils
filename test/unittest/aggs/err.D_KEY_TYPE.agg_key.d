/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: An aggregation (dynamic expression) cannot be used as
 *	aggregation key.
 *
 * SECTION: Aggregations/Aggregations
 */

BEGIN
{
	@a = count();
	@[@a] = count();
	exit(0);
}
