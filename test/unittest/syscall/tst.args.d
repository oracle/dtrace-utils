/*
 * Oracle Linux DTrace.
 * Copyright © 2007, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: syscall-tst-args */

/*
 * ASSERTION: Make sure we're correctly reporting arguments to syscall probes.
 */

#pragma D option quiet

syscall::mmap*:entry
/pid == $target && arg0 == 0 && arg1 == 1 && arg2 == 2 && arg3 == 3 &&
 (int)arg4 == -1 && arg5 == 0x12345678/
{
	exit(0);
}

tick-1s
/i++ == 3/
{
	exit(1);
}
