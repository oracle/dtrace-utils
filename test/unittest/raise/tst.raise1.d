/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */
/* @@trigger: raise-tst-raise1 */
/* @@trigger-timing: after */
/* @@runtest-opts: $_pid */

/*
 * ASSERTION:
 * 	Positive test for raise
 *
 * SECTION: Actions and Subroutines/raise()
 */

#pragma D option destructive

BEGIN
{
	/*
	 * Wait no more than five seconds for the process to call ioctl().
	 */
	timeout = timestamp + 5000000000;
	self->raised = 0;
}

syscall::ioctl:entry
/pid == $1 && !self->raised/
{
	trace("raised");
	raise(SIGINT);
	/*
	 * Wait no more than three seconds for the process to die.
	 */
	timeout = timestamp + 3000000000;
	self->raised = 1;
}

syscall::exit_group:entry
/pid == $1/
{
	exit(0);
}

profile:::tick-4
/timestamp > timeout/
{
	trace("timed out");
	exit(1);
}
