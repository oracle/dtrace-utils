/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *
 * tick-1sec simple test.
 *
 * SECTION: profile Provider/tick-n probes
 *
 */


#pragma D option quiet

tick-1sec
{
	printf("This test is a simple tick-sec provider test\n");
}
tick-1sec
{
	printf("This test is a simple tick-sec provider test");
	exit(0);
}
