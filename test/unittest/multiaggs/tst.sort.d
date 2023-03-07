/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@trigger: bogus-ioctl */

#pragma D option quiet

syscall::ioctl:entry
/pid == $target/
{
	i++;
	@a[i] = sum(100 - (i / 2));
	@b[i] = sum(100 - (i / 4));
	@c[i] = sum(100 - (i / 8));
	@d[i] = sum(100 - (i / 16));
}

syscall::ioctl:entry
/pid == $target && i == 100/
{
	printa("%10d %@10d %@10d %@10d %@10d\n", @a, @b, @c, @d);
	printa("%10d %@10d %@10d %@10d %@10d\n", @d, @c, @b, @a);
	exit(0);
}
