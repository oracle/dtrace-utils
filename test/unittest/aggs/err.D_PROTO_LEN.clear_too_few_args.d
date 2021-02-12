/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The clear() action requires one argument (agg).
 *
 * SECTION: Aggregations/Data Normalization
 */

BEGIN
{
	clear();
	exit(0);
}
