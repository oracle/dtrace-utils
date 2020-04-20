/*
 * Oracle Linux DTrace.
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 *   llquantize() should not have more than six arguments
 *
 * SECTION: Aggregations/Aggregations
 *
 */

BEGIN
{
	@a[1] = llquantize(10, 10, 2, 4, 10, 3, 5);
}
