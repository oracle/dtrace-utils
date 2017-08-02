/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Test using an aggregation in an expression context.
 *  This should result in a compile-time error.
 *
 * SECTION: Aggregations/Aggregations
 *
 */

BEGIN
{
	trace(@a + 3);
}
