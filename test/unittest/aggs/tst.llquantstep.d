/*
 * Oracle Linux DTrace.
 * Copyright (c) 2014, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:  A step value that evenly divides factor ** lmag,
 *             but not factor ** 2, works correctly.
 *
 * SECTION: Aggregations/Aggregations
 */

#pragma D option quiet

tick-1ms
{
	@ = llquantize(i++, 3, 3, 5, 27);
}

tick-1ms
/i == 1500/
{
	exit(0);
}
