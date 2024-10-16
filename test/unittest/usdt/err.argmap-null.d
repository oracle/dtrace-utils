/*
 * Oracle Linux DTrace.
 * Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: usdt-tst-argmap */
/* @@trigger-timing: before */
/* @@runtest-opts: $_pid */

/*
 * ASSERTION: Verify that a probe left with no args at all due to mapping
 *            has no args.
 */

BEGIN
{
	/* Timeout after 5 seconds */
	timeout = timestamp + 5000000000;
}

test_prov$1:::place4
/args[0] != 204 || args[0] == 204/
{
	printf("this should never happen");
	exit(1);
}

test_prov$1:::place4
{
	printf("nor should this");
	exit(1);
}

profile:::tick-1
/timestamp > timeout/
{
	trace("test timed out");
	exit(1);
}
