/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@xfail: dtv2 */
/* @@trigger: none */
/* @@timeout: 12 */

/*
 * Count off and report the number of seconds elapsed
 */
dtrace:::BEGIN
{
	i = 0;
}

profile:::tick-1sec
/i < 11/
{
	i = i + 1;
	trace(i);
}

profile:::tick-1sec
/i == 11/
{
	exit(0);
}

dtrace:::END
{
	trace(i);
}
