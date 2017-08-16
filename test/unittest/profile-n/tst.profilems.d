/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *
 * Simple profile-ms simple test.
 *
 * SECTION: profile Provider/profile-n probes
 *
 */


#pragma D option quiet

profile-1ms
{
	printf("This test is a simple profile-ms provider test");
	exit(0);
}
