/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Verify many speculations (single-CPU case).
 *
 * SECTION: Speculative Tracing
 */
/* @@trigger: bogus-ioctl */

#pragma D option quiet

BEGIN
{
	n = 0;
}

/*
 * Each firing, n is incremented.  Which clause is used rotates modulo 4.
 *   0: get a specid
 *   1: speculate some output
 *   2: speculate some other output
 *   3: commit (sometimes) or discard (usually)
 */

syscall::ioctl:entry
/ pid == $target && (n & 3) == 0 /
{
	i = speculation();
}

syscall::ioctl:entry
/ pid == $target && (n & 3) == 1 /
{
	speculate(i);
	printf("%4d %4d", n, i);
}

syscall::ioctl:entry
/ pid == $target && (n & 3) == 2 /
{
	speculate(i);
	printf("%4d hello world\n", n);
}

syscall::ioctl:entry
/ pid == $target && (n & 3) == 3 && (n & 63) == 3 /
{
	commit(i);
}

syscall::ioctl:entry
/ pid == $target && (n & 3) == 3 && (n & 63) != 3 /
{
	discard(i);
}

syscall::ioctl:entry
/ pid == $target && n++ >= 1000 /
{
	exit(0);
}
