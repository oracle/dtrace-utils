/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *
 * tick-msec simple test.
 *
 * SECTION: profile Provider/tick-n probes
 *
 */


#pragma D option quiet

tick-1msec
{
	printf("This test is a simple tick-msec provider test\n");
	exit(0);
}
