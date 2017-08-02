/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: simple fbt provider return test.
 *
 * SECTION: FBT Provider/Probe arguments
 */

#pragma D option quiet

BEGIN
{
	self->traceme = 1;
}

fbt:::entry
{
	printf("Entering the function\n");
}

fbt:::return
{
	printf("Returning the function\n");
	exit(0);
}
