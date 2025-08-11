/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, 2025, Oracle and/or its affiliates. All rights reserved.
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
 *	char	oldcomm[16]
 *	char	newcomm[16]
 *	short	oom_score_adj
 */
this char v;
this bool done;

sdt:task::task_rename
{
	printf("PID OK, oldcomm [%s], newcomm [%s], oom_score_adj %hd\n", stringof(args[0]), stringof(args[1]), args[2]);
	exit(0);
}

profile:::tick-1
/timestamp > timeout/
{
	trace("test timed out");
	exit(1);
}
