/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *   lquantize() should not have more than five (!) arguments
 *
 * SECTION: Aggregations/Aggregations
 *
 */

BEGIN
{
	@[1] = lquantize(10, 0, 100, 1, 10, 20);
}
