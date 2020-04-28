/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: lquantize() does not accept a non-scalar value
 *
 * SECTION: Aggregations/Aggregations
 */

BEGIN
{
	@ = lquantize("", 1, 5, 1);
	exit(0);
}
