/*
 * Oracle Linux DTrace.
 * Copyright © 2013, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: usdt-tst-args */
/* @@trigger-timing: before */
/* @@runtest-opts: $_pid */

/*
 * Ensure that arguments to USDT probes can be retrieved both from registers
 * and the stack.
 */
BEGIN
{
	/* Timeout after 5 seconds */
	timeout = timestamp + 5000000000;
}

test_prov$1:::place
/arg0 == 10 && arg1 ==  4 && arg2 == 20 && arg3 == 30 && arg4 == 40 &&
 arg5 == 50 && arg6 == 60 && arg7 == 70 && (int)arg8 == 80 && arg9 == 90/
{
	exit(0);
}

test_prov$1:::place
{
	printf("args are %d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n",
	       arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, (int)arg8, arg9);
	exit(1);
}

profile:::tick-1
/timestamp > timeout/
{
	trace("test timed out");
	exit(1);
}
