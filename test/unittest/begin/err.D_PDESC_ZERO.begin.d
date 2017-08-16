/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Arguments to BEGIN provider not allowed.
 *
 * SECTION: dtrace Provider
 *
 */


#pragma D option quiet

BEGIN::read:entry
{
	printf("Begin fired first\n");
	exit(0);
}
