/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2018, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@runtest-opts: $_pid */
/* @@trigger: pid-tst-ret1 */
/* @@trigger-timing: before */

/*
 * ASSERTION: test that we get the right return value from non-leaf returns
 *
 * SECTION: pid provider
 */

#pragma D option destructive

BEGIN
{
	/*
	 * Wait no more than a second for the first call to ioctl(2).
	 */
	timeout = timestamp + 1000000000;
}

syscall::ioctl:return
/pid == $1/
{
	i = 0;
	raise(SIGUSR1);
	/*
	 * Wait half a second after raising the signal.
	 */
	timeout = timestamp + 500000000;
}

pid$1:a.out:go:return
/arg1 == 101/
{
	exit(0);
}

pid$1:a.out:go:return
{
	printf("wrong return value: %d", arg1);
	exit(1);
}

profile:::tick-4
/timestamp > timeout/
{
	trace("test timed out");
	exit(1);
}
