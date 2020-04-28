/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: quantize() should not have more than two arguments
 *
 * SECTION: Aggregations/Aggregations
 */

BEGIN
{
	@ = quantize(1, 2, 3);
	exit(0);
}
