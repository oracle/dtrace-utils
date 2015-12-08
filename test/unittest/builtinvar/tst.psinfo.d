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

#pragma	ident	"%Z%%M%	%I%	%E% SMI"

/*
 * ASSERTION:
 * To print psinfo structure values from profile.
 *
 * SECTION: Variables/Built-in Variables
 */

/* @@timeout: 40 */

#pragma D option quiet

BEGIN
{
}

tick-10ms
{
	printf("number of lwps in process = %d\n", curpsinfo->pr_nlwp);
	printf("unique process id = %d\n", curpsinfo->pr_pid);
	printf("process id of parent = %d\n", curpsinfo->pr_ppid);
	printf("pid of process group leader = %d\n", curpsinfo->pr_pgid);
	printf("session id = %d\n", curpsinfo->pr_sid);
	printf("real user id = %d\n", curpsinfo->pr_uid);
	printf("effective user id = %d\n", curpsinfo->pr_euid);
	printf("real group id = %d\n", curpsinfo->pr_gid);
	printf("effective group id = %d\n", curpsinfo->pr_egid);
	printf("address of process = %p\n", curpsinfo->pr_addr);
	printf("address of controlling tty = %p\n", curpsinfo->pr_ttydev);
	printf("process name = %s\n", curpsinfo->pr_fname);
	/* These are still getting faked */
	printf("initial chars of arg list = %s\n", curpsinfo->pr_psargs);
	printf("wait status for zombie = %d\n", curpsinfo->pr_wstat);
	printf("initial argument count = %d\n", curpsinfo->pr_argc);
	printf("initial argument vector = %p\n", curpsinfo->pr_argv);
	printf("initial environment vector = %p\n", curpsinfo->pr_envp);
	printf("process data model = %d\n", curpsinfo->pr_dmodel);
	printf("task id = %d\n", curpsinfo->pr_taskid);
	printf("project id = %d\n", curpsinfo->pr_projid);
	printf("number of zombie LWPs = %d\n", curpsinfo->pr_nzomb);
	printf("pool id = %d\n", curpsinfo->pr_poolid);
	printf("zone id = %d\n", curpsinfo->pr_zoneid);
	printf("contract = %d\n", curpsinfo->pr_contract);
	exit (0);
}
