/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2006 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/* @@trigger: bogus-ioctl */
/* @@runtest-opts: $_pid */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#pragma D option destructive
#pragma D option quiet

syscall::ioctl:entry
/pid == $1 && arg0 == -1u/
{
	raise(SIGUSR1);
}

/*
 * On Linux, the ioctl prototype is
 *	extern int ioctl (int, unsigned long int, ...)
 * so under 64-bit architecture this means that -1 as first argument is
 * represented as -1u (32-bit) and -1 as second argument is -1 (64-bit).
 */
syscall::ioctl:entry
/pid == $1 && arg0 != -1u && arg1 == -1 && arg2 == NULL/
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
