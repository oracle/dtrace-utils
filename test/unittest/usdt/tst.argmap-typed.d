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
 * ASSERTION: Verify that args[N] variables are properly typed when mapped.
 */

BEGIN
{
	/* Timeout after 5 seconds */
	timeout = timestamp + 5000000000;
}

test_prov$1:::place2
/stringof(args[0]) != "foo" || args[1] != 255 || args[2] != 255 || stringof(args[3]) != "foo"/
{
	printf("args are %s, %d, %d, %s; should be \"foo\", 255, 255, \"foo\"",
	    stringof(args[0]), args[1], args[2], stringof(args[3]));
	exit(1);
}

test_prov$1:::place2
/stringof(copyinstr(arg0)) != "foo" || arg1 != 255 || arg2 != 255 || stringof(copyinstr(arg3)) != "foo"/
{
	printf("args are %s, %d, %d, %s; should be \"foo\", 255, 255, \"foo\"",
	    stringof(copyinstr(arg0)), arg1, arg2, stringof(copyinstr(arg3)));
	exit(1);
}

test_prov$1:::place2
{
	exit(0);
}

profile:::tick-1
/timestamp > timeout/
{
	trace("test timed out");
	exit(1);
}
