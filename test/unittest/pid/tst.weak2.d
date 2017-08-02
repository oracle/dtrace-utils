/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@runtest-opts: $_pid */
/* @@trigger: pid-tst-weak2 */
/* @@trigger-timing: after */

/*
 * ASSERTION: check that we prefer weak symbols to local ones
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

pid$1:a.out:go:entry,
pid$1:a.out:_go:entry
/probefunc == "_go"/
{
	i++;
}

pid$1:a.out:go:entry,
pid$1:a.out:_go:entry
/probefunc == "go"/
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
