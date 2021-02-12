/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The trunc() action requires one argument (agg).
 *
 * SECTION: Aggregations/Truncating Aggregations
 */

BEGIN
{
	trunc();
	exit(0);
}
