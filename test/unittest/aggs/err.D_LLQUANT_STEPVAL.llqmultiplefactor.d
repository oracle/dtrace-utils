/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  llquantize() steps must be a multiple of factor when steps>factor
 *
 * SECTION: Aggregations/Aggregations
 *
 */


BEGIN
{
	@ = llquantize(1234, 10, 0, 6, 15);
}
