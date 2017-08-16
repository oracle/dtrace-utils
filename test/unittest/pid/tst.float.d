/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@runtest-opts: $_pid */
/* @@trigger: pid-tst-float */
/* @@trigger-timing: after */

/*
 * ASSERTION: Make sure we can work on processes that use the FPU
 *
 * SECTION: pid provider
 */

BEGIN
{
	/*
	 * Let's just do this for 5 seconds.
	 */
	timeout = timestamp + 5000000000;
}

pid$1:a.out:main:
{}

profile:::tick-4
/timestamp > timeout/
{
	exit(0);
}

