/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The trunc() action accepts just 2 arguments.
 *
 * SECTION: Aggregations/Truncating Aggregations
 */

BEGIN
{
	@a = count();
	trunc(@a, 4, 1);
	exit(0);
}
