/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  This D ditty tests the ability to perform both copyin and copyinstr.  We
 *  grab the value or pr_envp, read the first pointer from the user stack,
 *  and then copyinstr the first environment variable and print it.
 *
 * SECTION: Actions and Subroutines/copyin();
 * 	Actions and Subroutines/copyinstr();
 *	User Process Tracing/copyin() and copyinstr()
 */

BEGIN
/curpsinfo->pr_dmodel == PR_MODEL_ILP32/
{
	envp = *(uint32_t *)copyin(curpsinfo->pr_envp, sizeof (uint32_t));
	printf("envp[0] = \"%s\"", copyinstr(envp));
	exit(0);
}

BEGIN
/curpsinfo->pr_dmodel == PR_MODEL_LP64/
{
	envp = *(uint64_t *)copyin(curpsinfo->pr_envp, sizeof (uint64_t));
	printf("envp[0] = \"%s\"", copyinstr(envp));
	exit(0);
}
