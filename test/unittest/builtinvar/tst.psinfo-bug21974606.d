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

#pragma	ident	"%Z%%M%	%I%	%E% SMI"

/*
 * ASSERTION:
 * To ensure pr_psargs does not have an (extra) trailing space.
 *
 * SECTION: Variables/Built-in Variables
 */

#pragma D option quiet
#pragma D option destructive

BEGIN
{
	system("/bin/ls >/dev/null");
}

exec-success
/execname == "ls"/
{
	psargs = stringof(curpsinfo->pr_psargs);
	printf("psargs [%s]\n", psargs);
	printf("Last char is [%c]\n", psargs[strlen(psargs) - 1]);
	exit(psargs[strlen(psargs) - 1] == ' ' ? 1 : 0);
}
