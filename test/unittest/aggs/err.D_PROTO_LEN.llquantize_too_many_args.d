/*
 * Oracle Linux DTrace.
 * Copyright (c) 2017, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: llquantize() should not have more than six arguments
 *
 * SECTION: Aggregations/Aggregations
 */

BEGIN
{
	@ = llquantize(10, 10, 2, 4, 10, 3, 5);
	exit(0);
}
