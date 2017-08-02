/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@no-xfail */

/*
 * ASSERTION:
 *
 * profile without valid value keyword just call with 'n'.
 *
 * SECTION: profile Provider/profile-n probes
 *
 */


#pragma D option quiet

profile-n
{
	printf("This test should fail\n");
	exit(1);
}
