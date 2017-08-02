/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: FBT provider entry/return self->var (TLS) lookup verification.
 *
 * SECTION: FBT Provider/Probes
 */

#pragma D option quiet
#pragma D option statusrate=10ms

BEGIN
{
	me = pid;
	num_entry = 0;
	num_return = 0;
	fails = 0;
}

syscall::ioctl:entry
/me == pid/
{
	num_entry++;
	self->token = pid;
}

fbt::SyS_ioctl:entry
/me == pid && num_entry > 0/
{
}

fbt::SyS_ioctl:return
/me == pid && num_entry > 0/
{
}

fbt::SyS_ioctl:return
/me == pid && num_entry > 0 && self->token != pid/
{
	fails++;
}

syscall::ioctl:return
/me == pid && num_entry > 0/
{
	num_return++;
}

syscall::ioctl:entry,
syscall::ioctl:return
/num_entry >= 10 && num_return >= 10/
{
	exit(fails ? 1 : 0);
}
