/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * When the aggrate is faster than the switchrate, each printa() should
 * reflect all of the counts so far.
 */

/* @@trigger: periodic_output */
/* @@sort */

#pragma D option quiet
#pragma D option switchrate=100ms
#pragma D option aggrate=50ms

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
