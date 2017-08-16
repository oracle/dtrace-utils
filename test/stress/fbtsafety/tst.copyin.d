/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@no-xfail */

/*
 * ASSERTION:
 *   We set our buffer size absurdly low to prevent a flood of errors that we
 *   don't care about.  We set our statusrate to be infinitely short to cause
 *   lots of activity by the DTrace process.
 *
 * SECTION: Actions and Subroutines/copyin();
 *	Options and Tunables/bufsize;
 *	Options and Tunables/bufpolicy;
 *	Options and Tunables/statusrate
 */


#pragma D option bufsize=16
#pragma D option bufpolicy=ring
#pragma D option statusrate=1nsec

syscall:::entry
{
	n++;
	trace(copyin(rand(), 1));
}

syscall:::entry
{
	trace(copyin(rand() | 1, 1));
}

syscall:::entry
{
	trace(copyin(NULL, 1));
}

dtrace:::ERROR
{
	err++;
}

tick-1sec
/sec++ == 10/
{
	exit(2);
}

END
/n == 0 || err == 0/
{
	exit(1);
}

END
/n != 0 && err != 0/
{
	exit(0);
}
