/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@xfail: dtv2 */
/* @@trigger: usdt-tst-args */
/* @@trigger-timing: before */
/* @@runtest-opts: $_pid */

/*
 * ASSERTION:
 *
 * SECTION:
 *
 * NOTES:
 *
 */
BEGIN
{
	/* Timeout after 5 seconds */
	timeout = timestamp + 5000000000;
}

test_prov$1:::place
/arg0 == 10 && arg1 == 4/
{
	exit(0);
}

test_prov$1:::place
{
	printf("args are %d, %d; should be 10, 4", arg0, arg1);
	exit(1);
}

profile:::tick-1
/timestamp > timeout/
{
	trace("test timed out");
	exit(1);
}
