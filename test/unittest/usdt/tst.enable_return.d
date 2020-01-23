/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@xfail: dtv2 */
/* @@trigger: usdt-tst-special 8 */

#pragma D option quiet

/*
 * ASSERTION:
 *	Make sure that an is-enabled probe can be used as argument to a return
 *	instruction (which may be turned into a tail-call on architectures that
 *	support it.
 */
BEGIN
{
	/* Timeout after 5 seconds */
	timeout = timestamp + 5000000000;
}

test_prov$target:::
{
	trace(probename);
}

profile:::tick-1
/timestamp > timeout/
{
	trace("test timed out");
	exit(1);
}
