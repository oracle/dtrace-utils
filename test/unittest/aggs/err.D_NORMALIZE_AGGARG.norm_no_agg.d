/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The first argument to normalize() should be an aggregation.
 *
 * SECTION: Aggregations/Data Normalization
 */

BEGIN
{
	normalize(count(), 4);
	exit(0);
}
