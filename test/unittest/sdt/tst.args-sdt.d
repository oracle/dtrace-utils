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

/* @@trigger: usdt-tst-args */
/* @@trigger-timing: before */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#pragma D option destructive

/*
 * Ensure that arguments to SDT probes can be retrieved both from registers and
 * the stack.
 */
BEGIN
{
	/* Timeout after 5 seconds */
	timeout = timestamp + 5000000000;
	system("ls >/dev/null");
}

sdt:::test
/arg0 == 10 && arg1 == 20 && arg2 == 30 && arg3 == 40 &&
 arg4 == 50 && arg5 == 60 && arg6 == 70 && arg7 == 80/
{
	exit(0);
}

sdt:::test
{
	printf("args are %d, %d, %d, %d, %d, %d, %d, %d\n",
	       arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
	exit(1);
}

profile:::tick-1
/timestamp > timeout/
{
	trace("test timed out");
	exit(1);
}
