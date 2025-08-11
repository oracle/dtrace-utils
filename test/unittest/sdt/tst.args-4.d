/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option destructive
#pragma D option quiet

/*
 * Ensure that arguments to SDT probes can be retrieved correctly.
 */
BEGIN
{
	/* Timeout after 5 seconds */
	timeout = timestamp + 5000000000;
	system("ls >/dev/null");
}

/*
 * The 'task_rename' tracepoint passes the following arguments:
 *
 *	pid_t	pid
 *	char	oldcomm[16]
 *	char	newcomm[16]
 *	short	oom_score_adj
 */
this char v;
this bool done;

sdt:task::task_rename
/args[0] == pid/
{
	printf("PID OK, oldcomm [%s], newcomm [%s], oom_score_adj %hd\n", stringof(args[1]), stringof(args[2]), args[3]);
	exit(0);
}

profile:::tick-1
/timestamp > timeout/
{
	trace("test timed out");
	exit(1);
}
