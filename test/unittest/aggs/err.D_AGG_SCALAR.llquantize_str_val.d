/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: llquantize() does not accept a non-scalar value
 *
 * SECTION: Aggregations/Aggregations
 */

BEGIN
{
	@ = llquantize("", 10, 0, 6, 20);
	exit(0);
}
