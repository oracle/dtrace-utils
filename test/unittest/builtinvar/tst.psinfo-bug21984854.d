/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

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
	argv = *(uint32_t *)copyin(curpsinfo->pr_argv, sizeof(uint32_t));
	printf("argv[0] = \"%s\"", copyinstr(argv));

	envp = *(uint32_t *)copyin(curpsinfo->pr_envp, sizeof(uint32_t));
	printf("envp[0] = \"%s\"", copyinstr(envp));
	exit(0);
}

BEGIN
/curpsinfo->pr_dmodel == PR_MODEL_LP64/
{
	argv = *(uint64_t *)copyin(curpsinfo->pr_argv, sizeof(uint64_t));
	printf("envp[0] = \"%s\"", copyinstr(argv));

	envp = *(uint64_t *)copyin(curpsinfo->pr_envp, sizeof(uint64_t));
	printf("envp[0] = \"%s\"", copyinstr(envp));
	exit(0);
}

ERROR
{
	exit(1);
}
