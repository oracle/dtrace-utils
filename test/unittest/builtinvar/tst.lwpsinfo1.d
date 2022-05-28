/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

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
	exit(0);
}

ERROR
{
	exit(1);
}
