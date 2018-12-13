/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2018, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@runtest-opts: $_pid */
/* @@trigger-timing: before */
/* @@trigger: pid-tst-float */

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

pid$1:a.out::return
{
	@[stack()] = sum(0);
}

pid$1:a.out::
{
	@[stack()] = sum(0);
}

pid$1:a.out::entry
{
	@[stack()] = sum(0);
}

profile:::tick-4
/timestamp > timeout/
{
	exit(0);
}
