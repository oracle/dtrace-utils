/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *
 * Negative test; calls tick without valid value.
 *
 * SECTION: profile Provider/tick-n probes
 *
 */


#pragma D option quiet

tick-n
{
	printf("This test is a simple tick-n negative test\n");
	exit(0);
}
