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
 * Copyright 2013 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma	ident	"%Z%%M%	%I%	%E% SMI"

/*
 * ASSERTION:
 * To print errno for failed system calls and make sure it succeeds, and is
 * correct.
 *
 * SECTION: Variables/Built-in Variables
 */

/* @@skip: known erratic */

#pragma D option quiet
#pragma D option destructive

BEGIN
{
	parent = pid;
	system("cat /non/existant/file");
}

syscall::open:entry
{
	this->fn = copyinstr(arg0);
}

syscall::open:return
/this->fn == "/non/existant/file" && errno != 0/
{
	printf("OPEN FAILED with errno %d for %s\n", errno, this->fn);
}

proc:::exit
/progenyof(parent)/
{
	printf("At exit, errno = %d\n", errno);
}

proc:::exit
/progenyof(parent)/
{
	exit(0);
}
