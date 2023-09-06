/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@trigger: bogus-ioctl */
/* @@nosort */

#pragma D option quiet

int64_t val, shift;

syscall::ioctl:entry
/pid == $target && val == 0/
{
	val = -(1 << shift);
	shift++;
}

syscall::ioctl:entry
/pid == $target && shift == 32/
{
	exit(0);
}

syscall::ioctl:entry
/pid == $target && val == -1/
{
	val = (1 << shift);
}

syscall::ioctl:entry
/pid == $target/
{
	@[shift] = quantize(val, val);
	val >>= 1;
}
