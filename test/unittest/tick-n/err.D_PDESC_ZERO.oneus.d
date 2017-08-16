/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *
 * Call profile-us; less than 200 micro seconds.
 *
 * SECTION: profile Provider/profile-n probes
 *
 */

#pragma D option quiet

profile-1us
{
	printf(" Calling profile-us less than 200 micro seconds should fail\n");
	exit (0);
}
