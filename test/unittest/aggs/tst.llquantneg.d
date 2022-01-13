/*
 * Oracle Linux DTrace.
 * Copyright (c) 2013, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: llquantize() handles a negative increment correctly.
 *
 * SECTION: Aggregations/Aggregations
 */
/* @@trigger: bogus-ioctl */

#pragma D option quiet

syscall::ioctl:entry
/i % 2 == 0/
{
	@ = llquantize(i++, 10, 0, 6, 20);
}

syscall::ioctl:entry
/i % 2 != 0/
{
	@ = llquantize(i++, 10, 0, 6, 20, -1);
}

syscall::ioctl:entry
/i == 1500/
{
	exit(0);
}
