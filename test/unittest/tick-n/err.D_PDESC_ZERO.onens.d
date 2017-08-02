/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *
 * Call profile-'ns' less than 200 micro seconds.
 *
 * SECTION: profile Provider/profile-n probes
 *
 */

#pragma D option quiet

profile-1ns
{
	printf("Calls 'ns' less than 200 micro seconds\n");
	exit (0);
}
