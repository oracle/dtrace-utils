/*
 * Oracle Linux DTrace.
 * Copyright (c) 2013, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Optional increment parameter to llquantize() functions correctly.
 *
 * SECTION: Aggregations/Aggregations
 */
/* @@trigger: bogus-ioctl */

#pragma D option quiet

syscall::ioctl:entry
/pid == $target/
{
	@ = llquantize(i++, 10, 0, 6, 20, 2);
}

syscall::ioctl:entry
/pid == $target && i == 1500/
{
	exit(0);
}
