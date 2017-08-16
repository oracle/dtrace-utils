/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *   Test redefining the aggregation
 *
 * SECTION: Aggregations/Aggregations
 *
 */

BEGIN
{
	@a = count();
}

END
{
	@a = max(0);
}
