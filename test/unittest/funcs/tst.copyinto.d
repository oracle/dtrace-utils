/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 * 	test copyinto by copying the first string of the user's
 *	environment.
 *
 * SECTION: Actions and Subroutines/copyinto()
 *
 */

#pragma D option quiet

BEGIN
/curpsinfo->pr_dmodel == PR_MODEL_ILP32/
{
	envp = alloca(sizeof(uint32_t));
	copyinto(curpsinfo->pr_envp, sizeof(uint32_t), envp);
	printf("envp[0] = \"%s\"", copyinstr(*(uint32_t *)envp));
	exit(0);
}

BEGIN
/curpsinfo->pr_dmodel == PR_MODEL_LP64/
{
	envp = alloca(sizeof(uint64_t));
	copyinto(curpsinfo->pr_envp, sizeof(uint64_t), envp);
	printf("envp[0] = \"%s\"", copyinstr(*(uint64_t *)envp));
	exit(0);
}
