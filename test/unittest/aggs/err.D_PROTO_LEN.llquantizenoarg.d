/*
 * Oracle Linux DTrace.
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	llquantize() should not accept a call with no arguments
 *
 * SECTION: Aggregations/Aggregations
 *
 */

BEGIN
{
	@a[1] = llquantize();
}

