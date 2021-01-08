/*
 * Oracle Linux DTrace.
 * Copyright (c) 2013, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 * To ensure built-in variable 'vtimestamp' has a sensible value
 *
 * SECTION: Variables/Built-in Variables
 */

#pragma D option quiet
#pragma D option destructive

BEGIN
{
	self->ts = timestamp;
	self->vt = vtimestamp;

	system("/bin/true");
}

proc:::create
{
	exit(0);
}

END
{
	exit((vtimestamp == 0)
		? -1
		: (vtimestamp - self->vt) <= (timestamp - self->ts) ? 0 : 1);
}

ERROR
{
	exit(1);
}
