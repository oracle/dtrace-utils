/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The second argument to normalize() should be a scalar.
 *
 * SECTION: Aggregations/Data Normalization
 */

BEGIN
{
	@a = count();
	normalize(@a, "a");
	exit(0);
}
