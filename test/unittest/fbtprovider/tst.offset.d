/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: FBT provider return value offset verification test.
 *
 * SECTION: FBT Provider/Probe arguments
 */

/* @@runtest-opts: -Z */
/* @@trigger: pid-tst-args1 */

#pragma D option quiet
#pragma D option statusrate=10ms

BEGIN
{
	self->traceme = 1;
}

fbt::SyS_ioctl:entry,
fbt::__arm64_sys_ioctl:entry,
fbt::__x64_sys_ioctl:entry
{
	printf("Entering the function\n");
}

fbt::SyS_ioctl:return,
fbt::__arm64_sys_ioctl:return,
fbt::__x64_sys_ioctl:return
{
	printf("The offset = %u\n", arg0);
	exit(0);
}
