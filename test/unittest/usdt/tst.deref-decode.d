/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: usdt-tst-deref-decode */
/* @@trigger-timing: before */
/* @@runtest-opts: $_pid */

#pragma D option quiet

BEGIN
{
	/* Timeout after 5 seconds */
	timeout = timestamp + 5000000000;
}

test_prov$1:::deref
/arg9 == 12 && (arg0 != -13 || arg1 != 4 || arg2 != 52 || arg3 != -1 ||
		arg4 != 12 || arg5 != -3 || arg6 != 48 || arg7 != 0)/
{
	printf("args are %d %d %d %d %d %d %d %d %d; should be -13 4 52 -1 12 -3 48 0 12", arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg9);
	exit(1);
}

test_prov$1:::deref
/arg9 == 12/
{
	exit(0);
}

profile:::tick-1
/timestamp > timeout/
{
	trace("test timed out");
	exit(1);
}
