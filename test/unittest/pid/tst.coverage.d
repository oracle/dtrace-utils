/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */
/* @@runtest-opts: $_pid */
/* @@trigger: pid-tst-args1 */
/* @@trigger-timing: before */

/*
 * ASSERTION: test that we can trace every instruction safely
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
{}

profile:::tick-4
/timestamp > timeout/
{
	exit(0);
}
