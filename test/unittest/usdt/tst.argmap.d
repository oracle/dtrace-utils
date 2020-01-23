/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@xfail: dtv2 */
/* @@trigger: usdt-tst-argmap */
/* @@trigger-timing: before */
/* @@runtest-opts: $_pid */

/*
 * ASSERTION: Verify that argN and args[N] variables are properly remapped.
 */

BEGIN
{
	/* Timeout after 5 seconds */
	timeout = timestamp + 5000000000;
}

test_prov$1:::place
/arg0 != 4 || arg1 != 10 || arg2 != 10 || arg3 != 4/
{
	printf("args are %d, %d, %d, %d; should be 4, 10, 10, 4",
	    arg0, arg1, arg2, arg3);
	exit(1);
}

test_prov$1:::place
/args[0] != 4 || args[1] != 10 || args[2] != 10 || args[3] != 4/
{
	printf("args are %d, %d, %d, %d; should be 4, 10, 10, 4",
	    args[0], args[1], args[2], args[3]);
	exit(1);
}

test_prov$1:::place
{
	exit(0);
}

profile:::tick-1
/timestamp > timeout/
{
	trace("test timed out");
	exit(1);
}
