/*
 * Linux DTrace
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#!/usr/sbin/dtrace -qs

/*
 *  SYNOPSIS
 *    sudo ./140provider_syscall.d
 *
 *  DESCRIPTION
 *    With the syscall provider, you can probe the entry to
 *    and return from any system call, which is useful for
 *    watching how a program moves between user space and the
 *    kernel.
 */

/* probe on entry to system call write() */
syscall::write:entry
{
	printf("write %d bytes to fd %d\n", args[2], args[0]);

	exit(0);
}
