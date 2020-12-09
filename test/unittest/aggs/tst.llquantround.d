/*
 * Oracle Linux DTrace.
 * Copyright (c) 2013, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  The height of the ASCII quantization bars is determined using rounding and
 *  not truncated integer arithmetic.
 *
 * SECTION: Aggregations/Aggregations
 */

#pragma D option quiet

tick-10ms
/i++ >= 27/
{
	exit(0);
}

tick-10ms
/i > 8/
{
	@ = llquantize(2, 10, 0, 2, 20);
}

tick-10ms
/i > 2 && i <= 8/
{
	@ = llquantize(1, 10, 0, 2, 20);
}

tick-10ms
/i <= 2/
{
	@ = llquantize(0, 10, 0, 2, 20);
}
