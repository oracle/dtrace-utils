/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@runtest-opts: $_pid */
/* @@trigger: pid-tst-args1 */
/* @@trigger-timing: after */

/*
 * ASSERTION: test that all 10 arguments are what we expect them to be.
 *
 * SECTION: pid provider
 */

#pragma D option destructive

BEGIN
{
	/*
	 * Wait no more than a second for the first call to getpid(2).
	 */
	timeout = timestamp + 1000000000;
}

syscall::getpid:return
/pid == $1/
{
	i = 0;
	raise(SIGUSR1);
	/*
	 * Wait half a second after raising the signal.
	 */
	timeout = timestamp + 500000000;
}

pid$1:a.out:go:entry
/arg0 == 0 && arg1 == 1 && arg2 == 2 && arg3 == 3 && arg4 == 4 &&
arg5 == 5 && arg6 == 6 && arg7 == 7 && arg8 == 8 && arg9 == 9/
{
	exit(0);
}

pid$1:a.out:go:entry
{
	printf("wrong args: %d %d %d %d %d %d %d %d %d %d", arg0, arg1, arg2,
	    arg3, arg4, arg5, arg6, arg7, arg8, arg9);
	exit(1);
}

profile:::tick-4
/timestamp > timeout/
{
	trace("test timed out");
	exit(1);
}
