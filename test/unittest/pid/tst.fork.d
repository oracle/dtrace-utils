/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@runtest-opts: $_pid */
/* @@trigger: pid-tst-fork */
/* @@trigger-timing: after */

/*
 * ASSERTION: make sure fork(2) is okay
 *
 * SECTION: pid provider
 */

#pragma D option destructive

pid$1:a.out:waiting:entry
{
	this->value = (int *)alloca(sizeof (int));
	*this->value = 1;
	copyout(this->value, arg0, sizeof (int));
}

proc:::create
/pid == $1/
{
	child = args[0]->pr_pid;
	trace(pid);
}

pid$1:a.out:go:
/pid == child/
{
	trace("wrong pid");
	exit(1);
}

proc:::exit
/pid == $1 || pid == child/
{
	out++;
	trace(pid);
}

proc:::exit
/out == 2/
{
	exit(0);
}


BEGIN
{
	/*
	 * Let's just do this for 5 seconds.
	 */
	timeout = timestamp + 5000000000;
}

profile:::tick-4
/timestamp > timeout/
{
	trace("test timed out");
	exit(1);
}

