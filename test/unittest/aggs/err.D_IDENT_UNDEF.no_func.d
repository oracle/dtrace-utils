/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: An aggregation cannot be used as a statement.
 *
 * SECTION: Aggregations/Aggregations
 */

BEGIN
{
	@a;
	exit(0);
}
