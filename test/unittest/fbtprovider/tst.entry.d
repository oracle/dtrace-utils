/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: FBT provider entry probe support
 *
 * DEPENDENCY: futex syscall entry and return probe
 *
 * SECTION: FBT Provider/Probes
 */

/* @@runtest-opts: -Z */
/* @@trigger: futex */

#pragma D option quiet
#pragma D option statusrate=10ms

BEGIN
{
	num_entry = 0;
}

syscall::futex:entry
/pid == $target/
{
	num_entry++;
}

fbt::SyS_futex:entry,
fbt::__x64_sys_futex:entry,
fbt::__arm64_sys_futex:entry
/pid == $target && num_entry > 0/
{
}

syscall::futex:return
/pid == $target && num_entry > 0/
{
	exit(0);
}
