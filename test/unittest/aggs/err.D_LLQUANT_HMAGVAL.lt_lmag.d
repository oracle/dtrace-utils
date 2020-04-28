/*
 * Oracle Linux DTrace.
 * Copyright (c) 2013, 2020 Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: llquantize() high magnitude must be at least low magnitude.
 *
 * SECTION: Aggregations/Aggregations
 */

BEGIN
{
	@ = llquantize(1, 10, 4, 3, 20);
	exit(0);
}
