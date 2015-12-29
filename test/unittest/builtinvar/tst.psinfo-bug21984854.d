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

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * ASSERTION:
 *  Ensure that copyin() works on reading from the argv and envp arrays,
 *  and dereferencing elements from them using copyinstr().
 *
 * SECTION: Actions and Subroutines/copyin();
 * 	Actions and Subroutines/copyinstr();
 *	User Process Tracing/copyin() and copyinstr()
 */

BEGIN
/curpsinfo->pr_dmodel == PR_MODEL_ILP32/
{
	argv = *(uint32_t *)copyin(curpsinfo->pr_argv, sizeof (uint32_t));
	printf("argv[0] = \"%s\"", copyinstr(argv));

	envp = *(uint32_t *)copyin(curpsinfo->pr_envp, sizeof (uint32_t));
	printf("envp[0] = \"%s\"", copyinstr(envp));
	exit(0);
}

BEGIN
/curpsinfo->pr_dmodel == PR_MODEL_LP64/
{
	argv = *(uint64_t *)copyin(curpsinfo->pr_argv, sizeof (uint64_t));
	printf("envp[0] = \"%s\"", copyinstr(argv));

	envp = *(uint64_t *)copyin(curpsinfo->pr_envp, sizeof (uint64_t));
	printf("envp[0] = \"%s\"", copyinstr(envp));
	exit(0);
}

ERROR
{
	exit(1);
}
