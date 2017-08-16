/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * Test to ensure that invalid stores to a global associative array
 * are caught correctly.
 */

#pragma D option quiet

int last_cmds[int][4];

BEGIN
{
	errors = 0;
	forward = 0;
	backward = 0;
}

tick-1s
/!forward/
{
	forward = 1;
	last_cmds[1][4] = 0xdeadbeef;
}

tick-1s
/!backward/
{
	backward = 1;
	last_cmds[1][-5] = 0xdeadbeef;
}

tick-1s
/errors > 1/
{
	exit(0);
}

tick-1s
/n++ > 5/
{
	exit(1);
}

ERROR
/arg4 == DTRACEFLT_BADADDR/
{
	errors++;
}
