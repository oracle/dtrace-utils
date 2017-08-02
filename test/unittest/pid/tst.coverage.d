/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@runtest-opts: $_pid */

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
