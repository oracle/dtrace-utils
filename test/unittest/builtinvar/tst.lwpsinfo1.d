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
 * To print lwpsinfo_t structure values.
 *
 * SECTION: Variables/Built-in Variables
 */

#pragma D option quiet

BEGIN
{
	printf("The current lwp pr_flag is %d\n", curlwpsinfo->pr_flag);
	printf("The current lwp lwpid is %d\n", curlwpsinfo->pr_lwpid);
	printf("The current lwp internal address is %u\n",
			curlwpsinfo->pr_addr);
	printf("The current lwp state is %d\n", curlwpsinfo->pr_state);
	printf("The printable character for pr_state %c\n",
		curlwpsinfo->pr_sname);
	printf("The current lwp has priority %d\n", curlwpsinfo->pr_pri);
	printf("The current lwp is named %s\n", curlwpsinfo->pr_name);
	printf("The current lwp is on cpu %d\n", curlwpsinfo->pr_onpro);
	exit (0);
}
