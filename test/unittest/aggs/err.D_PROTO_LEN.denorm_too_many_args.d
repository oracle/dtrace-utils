/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The denormalize() action accepts only one argument (agg).
 *
 * SECTION: Aggregations/Data Normalization
 */

BEGIN
{
	@a = count();
	denormalize(@a, 1);
	exit(0);
}
