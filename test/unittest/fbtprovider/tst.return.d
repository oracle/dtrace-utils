/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: fbt provider return probe
 *
 * SECTION: FBT Provider/Probe arguments
 */

/* @@runtest-opts: -Z */
/* @@trigger: pid-tst-args1 */

#pragma D option quiet
#pragma D option statusrate=10ms

fbt::SyS_ioctl:return,
fbt::__arm64_sys_ioctl:return,
fbt::__x64_sys_ioctl:return
{
	exit(0);
}
