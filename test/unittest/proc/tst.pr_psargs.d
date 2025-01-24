/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: psinfo->pr_psargs provides correct results.
 */

#pragma D option destructive
#pragma D option quiet

BEGIN
{
	mypid = pid;
	system("/bin/echo TEST a b c ");
}

proc:::exec-success
/progenyof(mypid) && curpsinfo->pr_psargs == "/bin/echo TEST a b c"/
{
	trace(curpsinfo->pr_psargs);
	exit(0);
}

tick-1s
{
	exit(1);
}

ERROR
{
	exit(1);
}
