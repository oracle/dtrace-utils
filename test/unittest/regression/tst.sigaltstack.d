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
 * Copyright 2015 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma D option destructive
#pragma D option quiet

/*
 * Ensure that tracing sigaltstack syscalls works.
 */
BEGIN
{
	/* Timeout after 3 seconds */
	timeout = timestamp + 3000000000;
	system("vi -c :q tmpfile >/dev/null 2>&1 &");
}

syscall::sigaltstack:entry
{
	exit(0);
}

profile:::tick-100ms
/timestamp > timeout/
{
	trace("test timed out");
	exit(1);
}
