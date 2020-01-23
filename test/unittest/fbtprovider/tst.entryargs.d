/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: FBT provider entry probe args retrieval and verification
 *
 * SECTION: FBT Provider/Probes
 */

/* @@xfail: dtv2 */
/* @@runtest-opts: -Z */

#pragma D option quiet
#pragma D option statusrate=10ms

BEGIN
{
	me = pid;
	num_entry = 0;
}

syscall::futex:entry
/me == pid/
{
	num_entry++;
	self->arg0 = arg0;
	self->arg1 = arg1;
	self->arg2 = arg2;
	self->arg3 = arg3;
	self->arg4 = arg4;
	self->arg5 = arg5;
}

syscall::futex:entry
/me == pid && num_entry > 0/
{
	printf("args %x %x %x %x %x %x\n", arg0, arg1, arg2, arg3, arg4, arg5);
}

fbt::SyS_futex:entry,
fbt::__x64_sys_futex:entry
/me == pid && num_entry > 0/
{
	printf("args %x %x %x %x %x %x\n", arg0, arg1, arg2, arg3, arg4, arg5);
}

fbt::SyS_futex:entry,
fbt::__x64_sys_futex:entry
/me == pid && num_entry > 0 && self->arg0 != arg0/
{
	printf("arg0 mismatch: %x vs %x\n", self->arg0, arg0);
}

fbt::SyS_futex:entry,
fbt::__x64_sys_futex:entry
/me == pid && num_entry > 0 && self->arg1 != arg1/
{
	printf("arg1 mismatch: %x vs %x\n", self->arg1, arg1);
}

fbt::SyS_futex:entry,
fbt::__x64_sys_futex:entry
/me == pid && num_entry > 0 && self->arg2 != arg2/
{
	printf("arg2 mismatch: %x vs %x\n", self->arg2, arg2);
}

fbt::SyS_futex:entry,
fbt::__x64_sys_futex:entry
/me == pid && num_entry > 0 && self->arg3 != arg3/
{
	printf("arg3 mismatch: %x vs %x\n", self->arg3, arg3);
}

fbt::SyS_futex:entry,
fbt::__x64_sys_futex:entry
/me == pid && num_entry > 0 && self->arg4 != arg4/
{
	printf("arg4 mismatch: %x vs %x\n", self->arg4, arg4);
}

fbt::SyS_futex:entry,
fbt::__x64_sys_futex:entry
/me == pid && num_entry > 0 && self->arg5 != arg5/
{
	printf("arg5 mismatch: %x vs %x\n", self->arg5, arg5);
}

fbt::SyS_ioctl:return,
fbt::__x64_sys_ioctl:return
/me == pid && num_entry > 0/
{
	exit(0);
}
