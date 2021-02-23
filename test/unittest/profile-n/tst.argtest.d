/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *
 * Either one of arg0 (or) arg1 should be 0 and non-zero.
 *
 * SECTION: profile Provider/tick-n probes
 *
 */


#pragma D option quiet

tick-1
/(arg0 != 0 && arg1 == 0) || (arg0 == 0 && arg1 != 0)/
{
	printf("Test passed; either arg0/arg1 is zero\n");
	exit(0);
}

tick-1
/(arg0 == 0 && arg1 == 0) || (arg0 != 0 && arg1 != 0)/
{
	printf("Test failed; either arg0 (or) arg1 should be non zero\n");
	exit(1);
}
