/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: usdt-tst-argmap */
/* @@trigger-timing: before */
/* @@runtest-opts: $_pid */

/*
 * ASSERTION: Verify that args[N] variables are properly typed when mapped,
 *            even if some args are unused.
 */

BEGIN
{
	/* Timeout after 5 seconds */
	timeout = timestamp + 5000000000;
}

test_prov$1:::place3
/stringof(args[0]) != "bar"/
{
	printf("arg is %s; should be \"bar\"",
	    stringof(args[0]));
	exit(1);
}

test_prov$1:::place3
/stringof(copyinstr(arg0)) != "bar"/
{
	printf("arg is %s; should be \"bar\"",
	    stringof(copyinstr(arg0)));
	exit(1);
}

test_prov$1:::place3
{
	exit(0);
}

profile:::tick-1
/timestamp > timeout/
{
	trace("test timed out");
	exit(1);
}
