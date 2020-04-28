/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: lquantize() should have et least 3 arguments
 *
 * SECTION: Aggregations/Aggregations
 */

BEGIN
{
	@ = lquantize(1, 0);
	exit(0);
}
