/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Number of quantization levels must not be 0
 *
 * SECTION: Aggregations/Aggregations
 */

BEGIN
{
	@ = lquantize(1, 2, 3, 4);
	exit(0);
}
