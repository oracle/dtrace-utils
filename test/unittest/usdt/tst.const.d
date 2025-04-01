/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: usdt-tst-arg-const */
/* @@trigger-timing: before */
/* @@runtest-opts: $_pid */

#pragma D option quiet

BEGIN
{
	/* Timeout after 5 seconds */
	timeout = timestamp + 5000000000;
}

test_prov$1:::sval*,
test_prov$1:::uval*
/firings[probename]++/
{
	exit(0);
}

test_prov$1:::sval*,
test_prov$1:::uval*
{
	printf("%s: arg is %d\n", probename, arg0);
}

profile:::tick-1
/timestamp > timeout/
{
	trace("test timed out");
	exit(1);
}
