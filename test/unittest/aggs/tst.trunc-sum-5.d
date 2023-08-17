/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@trigger: bogus-ioctl */
/* @@nosort */

#pragma D option quiet

BEGIN
{
	i = 0;
	nsecs = 1000000000 * (long long)3;
	tstop = timestamp + nsecs;
}

syscall::ioctl:entry
/pid == $target && i < 100/
{
	@[100 + i] = sum(1000 + i);
        i++;
}

syscall::ioctl:entry
/pid == $target && i == 100/
{
	trunc(@, 5);
	tstop = timestamp + nsecs;
        i++;
}

syscall::ioctl:entry
/pid == $target && i > 100 && timestamp > tstop/
{
	@[200 + i] = sum(2000 + i);
        exit(0);
}
