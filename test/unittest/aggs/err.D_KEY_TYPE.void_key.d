/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: A void expression cannot be used as aggregation key.
 *
 * SECTION: Aggregations/Aggregations
 */

BEGIN
{
	@[trace(0)] = count();
	exit(0);
}
