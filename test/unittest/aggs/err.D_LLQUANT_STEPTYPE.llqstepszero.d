/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  llquantize() steps must be nonzero.
 *
 * SECTION: Aggregations/Aggregations
 *
 */


BEGIN
{
	@ = llquantize(timestamp, 10, 0, 6, 0);
}
