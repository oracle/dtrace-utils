/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * Don't time out until the buffer fills.  That might take some time, so
 * set the timeout to a high value to, in effect, disable it.
 */
/* @@xfail: dtv2 */
/* @@timeout: 600 */

#pragma D option bufsize=32m
#pragma D option bufpolicy=fill
#pragma D option destructive
#pragma D option quiet

BEGIN
/0 == 1/
{
	@ = count();
}

BEGIN
{
	freopen("/dev/null");
}

profile-4000hz
{
	printa(@);
	printa(@);
	printa(@);
	printa(@);
	printa(@);
	printa(@);
	printa(@);
	printa(@);
	printa(@);
	printa(@);
	printa(@);
	printa(@);
	printa(@);
	printa(@);
	printa(@);
	printa(@);
	printa(@);
	printa(@);
	printa(@);
	printa(@);
	printa(@);
	printa(@);
	printa(@);
	printa(@);
	printa(@);
	printa(@);
	printa(@);
	printa(@);
}

