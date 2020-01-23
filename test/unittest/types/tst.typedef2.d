/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 *   Declares a dynamic type and then uses it to copyin the first three
 *   environment variable pointers from the current process.
 *
 * SECTION: Structs and Unions/Structs;
 *	Actions and Subroutines/copyin();
 * 	Actions and Subroutines/copyinstr();
 *	Variables/External Variables
 *
 * NOTES:
 *  This test program declares a dynamic type and then uses it to copyin the
 *  first three environment variable pointers from the current process.  We
 *  then use the dynamic type to access the result of our copyin().  The
 *  special "D" module type scope is also tested.  This is similar to our
 *  struct declaration test but here we use a typedef.
 */

#pragma D option quiet

typedef struct env_vars_32 {
	uint32_t e1;
	uint32_t e2;
	uint32_t e3;
} env_vars_32_t;

typedef struct env_vars_64 {
	uint64_t e1;
	uint64_t e2;
	uint64_t e3;
} env_vars_64_t;

BEGIN
/curpsinfo->pr_dmodel == PR_MODEL_ILP32/
{
	e32 = (D`env_vars_32_t *)curpsinfo->pr_envp;

	printf("e1 = \"%s\"\n", stringof(copyinstr(e32->e1)));
	printf("e2 = \"%s\"\n", stringof(copyinstr(e32->e2)));
	printf("e3 = \"%s\"\n", stringof(copyinstr(e32->e3)));

	exit(0);
}

BEGIN
/curpsinfo->pr_dmodel == PR_MODEL_LP64/
{
	e64 = (D`env_vars_64_t *)curpsinfo->pr_envp;

	printf("e1 = \"%s\"\n", stringof(copyinstr(e64->e1)));
	printf("e2 = \"%s\"\n", stringof(copyinstr(e64->e2)));
	printf("e3 = \"%s\"\n", stringof(copyinstr(e64->e3)));

	exit(0);
}
