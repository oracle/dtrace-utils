/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@xfail: no msgsize or msgdsize on Linux */

/*
 * ASSERTION:
 * 	Make sure that the msgdsize safe to use at every fbt probe
 *
 * SECTION: Actions and Subroutines/msgdsize();
 *	Options and Tunables/bufsize;
 * 	Options and Tunables/bufpolicy;
 * 	Options and Tunables/statusrate
 */

#pragma D option bufsize=1000
#pragma D option bufpolicy=ring
#pragma D option statusrate=10ms

fbt:::
{
	on = (timestamp / 1000000000) & 1;
}

fbt:::
/on/
{
	trace(msgdsize((mblk_t *)rand()));
}

fbt:::entry
/on/
{
	trace(msgdsize((mblk_t *)arg1));
}

tick-1sec
/n++ == 20/
{
	exit(0);
}
