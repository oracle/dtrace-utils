/*
 * Oracle Linux DTrace.
 * Copyright (c) 2013, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: llquantize() low magnitude must be an integer constant.
 *
 * SECTION: Aggregations/Aggregations
 */

BEGIN
{
	lmag = 1;
	@ = llquantize(1, 10, lmag, 6, 20);
	exit(0);
}
