/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@runtest-opts: $_pid */
/* @@trigger: pid-tst-weak1 */
/* @@trigger-timing: before */

/*
 * ASSERTION: test that we prefer symbols with fewer underscores
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

pid$1:a.out:go:entry,
pid$1:a.out:_go:entry
/probefunc == "go"/
{
	i++;
}

pid$1:a.out:go:entry,
pid$1:a.out:_go:entry
/probefunc == "_go"/
{
	trace("resolved to '_go' rather than 'go'");
	exit(1);
}

pid$1:a.out:go:entry,
pid$1:a.out:_go:entry
/i == 2/
{
	exit(0);
}

profile:::tick-4
/timestamp > timeout/
{
	trace("test timed out");
	exit(1);
}
