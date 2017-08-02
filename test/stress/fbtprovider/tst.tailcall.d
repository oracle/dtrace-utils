/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@xfail: not yet ported */

/*
 * ASSERTION: simple fbt provider tailcall test.
 *
 * SECTION: FBT Provider/Probe arguments
 */

#pragma D option quiet
#pragma D option statusrate=10ms

fbt::ioctl:entry
{
	self->traceme = 1;
}

fbt:::entry
/self->traceme/
{
	printf("called %s\n", probefunc);
}

fbt::ioctl:return
/self->traceme/
{
	self->traceme = 0;
	exit (0);
}
