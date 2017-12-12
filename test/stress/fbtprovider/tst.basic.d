/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: simple fbt arguments test.
 *
 * SECTION: FBT Provider/Probes
 */

#pragma D option quiet

tick-1
{
	self->traceme = 1;
}

fbt:::
/self->traceme/
{
	printf("The arguments are %u %u %u %u %u %u %u %u %u\n", arg0, arg1,
	       arg2, arg3, arg4, arg5, arg6, arg7, arg8);
	exit (0);
}
