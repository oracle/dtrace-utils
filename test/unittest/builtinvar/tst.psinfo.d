/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * To print psinfo structure values from profile.
 *
 * SECTION: Variables/Built-in Variables
 */

/* @@timeout: 40 */

#pragma D option quiet

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

ERROR
{
	exit(1);
}
