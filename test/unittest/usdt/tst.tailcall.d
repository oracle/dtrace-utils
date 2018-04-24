/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: usdt-tst-special 1 */

#pragma D option quiet

/*
 * ASSERTION:
 *	Make sure that if a probe is rendered into a tail call by the compiler,
 *	we can still use it.  Fo architectures that do not compile the trigger
 *	code into a tail-call scenario, this will work as a regular USDT probe
 *	test as last instruction of a function.
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
