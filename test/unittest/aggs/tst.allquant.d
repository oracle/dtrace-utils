/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@trigger: bogus-ioctl */
/* @@nosort */

#pragma D option quiet

int i;

syscall::ioctl:entry
/pid == $target/
{
	@ = quantize(1LL << i);
	@ = quantize((1LL << i) + 1);
	@ = quantize(-(1LL << i));
	@ = quantize(-(1LL << i) - 1);
	i++;
}

syscall::ioctl:entry
/pid == $target && i == 64/
{
	exit(0);
}
