/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * When the aggrate is slower than the switchrate and the pace of printa()
 * actions, multiple printa() should all reflect the same stale count.
 */
/* @@trigger: periodic_output */
/* @@nosort */

#pragma D option quiet
#pragma D option switchrate=100ms
#pragma D option aggrate=4500ms

syscall::write:entry
/pid == $target/
{
	@ = count();
	printa(@);
}

syscall::write:entry
/pid == $target && n++ >= 7/
{
	exit(0);
}
