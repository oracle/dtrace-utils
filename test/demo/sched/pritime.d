/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@runtest-opts: $_pid 1 */
/* @@xfail: needs a triger */

#pragma D option quiet

BEGIN
{
	start = timestamp;
}

sched:::change-pri
/args[1]->pr_pid == $1 && args[0]->pr_lwpid == $2/
{
	printf("%d %d\n", timestamp - start, args[2]);
}

tick-1sec
/++n == 5/
{
	exit(0);
}
