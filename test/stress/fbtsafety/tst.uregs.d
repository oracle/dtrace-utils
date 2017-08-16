/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Trace uregs[0] at every fbt probe point.
 * SECTION: Variables/Built-in Variables;
 *      Options and Tunables/bufsize;
 *      Options and Tunables/bufpolicy;
 *      Options and Tunables/statusrate
 */


#pragma D option bufsize=1000
#pragma D option bufpolicy=ring
#pragma D option statusrate=10ms

fbt:::
{
	trace(uregs[0]);
}

tick-1sec
/n++ == 10/
{
	exit(0);
}
