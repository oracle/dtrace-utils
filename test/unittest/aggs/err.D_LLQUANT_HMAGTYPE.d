/*
 * Oracle Linux DTrace.
 * Copyright (c) 2013, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: llquantize() high magnitude must be an integer constant.
 *
 * SECTION: Aggregations/Aggregations
 */

BEGIN
{
	hmag = 10;
	@ = llquantize(1, 10, 0, hmag, 20);
	exit(0);
}
