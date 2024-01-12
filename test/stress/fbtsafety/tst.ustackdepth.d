/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *    Test that the ustackdepth variable is safe to use at every fbt probe
 *    point.
 *
 * SECTION: Variables/Built-in Variables;
 *	Options and Tunables/bufsize;
 * 	Options and Tunables/bufpolicy;
 * 	Options and Tunables/statusrate
 */

#pragma D option bufsize=1000
#pragma D option bufpolicy=ring
#pragma D option statusrate=10ms

fbt:::
{
	trace(ustackdepth);
}

tick-1sec
/n++ == 10/
{
	exit(0);
}
