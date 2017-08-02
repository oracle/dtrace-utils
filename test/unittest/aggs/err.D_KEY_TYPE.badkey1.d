/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Test the use of a void expression as an aggregation key.
 *  This should result in a compile-time error.
 *
 * SECTION: Aggregations/Aggregations
 *
 */

BEGIN
{
	@a[1, (void)0] = count();
}
