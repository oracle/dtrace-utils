/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2018, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@runtest-opts: $_pid */
/* @@trigger: pid-tst-gcc */
/* @@trigger-timing: before */

/*
 * ASSERTION: test that we can trace every instruction safely for gcc
 * compiled apps.
 *
 * SECTION: pid provider
 *
 */

BEGIN
{
	/*
	 * Let's just do this for 2 seconds.
	 */
	timeout = timestamp + 2000000000;
}

pid$1:a.out::
{
	@p[probemod, probefunc, probename] = count();
}

profile:::tick-4
/timestamp > timeout/
{
	exit(0);
}
