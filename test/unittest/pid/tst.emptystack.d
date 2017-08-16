/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@runtest-opts: $_pid */

/*
 * ASSERTION: the stack() action should be empty for all pid probes
 *
 * SECTION: pid provider
 */

#pragma D option quiet

BEGIN
{
	/*
	 * Monitor the program for two seconds.
	 */
	timeout = timestamp + 1000000000 * 2;
}

pid$1:::return
{
	@[stack()] = sum(0);
}

pid$1:a.out::
{
	@[stack()] = sum(0);
}

pid$1:::entry
{
	@[stack()] = sum(0);
}

profile:::tick-4
/timestamp > timeout/
{
	exit(0);
}
