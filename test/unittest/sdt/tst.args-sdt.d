/*
 * Oracle Linux DTrace.
 * Copyright © 2013, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: usdt-tst-args */
/* @@trigger-timing: before */

#pragma D option destructive

/*
 * Ensure that arguments to SDT probes can be retrieved both from registers and
 * the stack.
 */
BEGIN
{
	/* Timeout after 5 seconds */
	timeout = timestamp + 5000000000;
	system("ls >/dev/null");
}

sdt:::test
/arg0 == 10 && arg1 == 20 && arg2 == 30 && arg3 == 40 &&
 arg4 == 50 && arg5 == 60 && arg6 == 70 && arg7 == 80/
{
	exit(0);
}

sdt:::test
{
	printf("args are %d, %d, %d, %d, %d, %d, %d, %d\n",
	       arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
	exit(1);
}

profile:::tick-1
/timestamp > timeout/
{
	trace("test timed out");
	exit(1);
}
