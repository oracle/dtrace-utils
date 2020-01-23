/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 * A clause cannot contain a speculate after a commit.
 *
 * SECTION: Speculative Tracing/Committing a Speculation;
 *	Options and Tunables/cleanrate
 *
 */
#pragma D option quiet
#pragma D option cleanrate=3000hz

BEGIN
{
	self->i = 0;
	var1 = 0;
	var1 = speculation();
	printf("Speculation ID: %d\n", var1);
}

profile:::tick-1sec
/var1/
{
	speculate(var1);
	printf("Speculating on id: %d\n", var1);
	self->i++;
}

profile:::tick-1sec
/(!self->i)/
{
}

profile:::tick-1sec
/(self->i)/
{
	commit(var1);
	speculate(var1);
	exit(0);
}

ERROR
{
	exit(0);
}
