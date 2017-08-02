/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  min() should not accept more than one argument
 *
 * SECTION: Aggregations/Aggregations
 *
 */

BEGIN
{
	@a[1] = min(1, 2);
}

