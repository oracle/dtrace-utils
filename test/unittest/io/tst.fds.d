/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: bogus-ioctl */
/* @@trigger-timing: after */
/* @@runtest-opts: -C $_pid */

#pragma D option destructive
#pragma D option quiet

syscall::ioctl:entry
#if defined(ARCH_x86_64)
/pid == $1 && arg0 == -1u/
#else
/pid == $1 && arg0 == -1/
#endif
{
	raise(SIGUSR1);
}

syscall::ioctl:entry
#if defined(ARCH_x86_64)
/pid == $1 && arg0 != -1u && arg1 == -1 && arg2 == NULL/
#else
/pid == $1 && arg0 != -1 && arg1 == -1 && arg2 == NULL/
#endif
{
	printf("fds[%d] fi_name = %s\n", arg0, fds[arg0].fi_name);
	printf("fds[%d] fi_dirname = %s\n", arg0, fds[arg0].fi_dirname);
	printf("fds[%d] fi_pathname = %s\n", arg0, fds[arg0].fi_pathname);
	printf("fds[%d] fi_mount = %s\n", arg0, fds[arg0].fi_mount);
	printf("fds[%d] fi_offset = %d\n", arg0, fds[arg0].fi_offset);
	printf("fds[%d] fi_oflags = %x\n", arg0, fds[arg0].fi_oflags);
}

proc:::exit
/pid == $1/
{
	exit(0);
}
