/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Call to commit() on a buffer after it has been discarded is silently
 * ignored.
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
	self->commit = 0;
	self->discard = 0;
	var1 = speculation();
	printf("Speculation ID: %d\n", var1);
}

BEGIN
/var1/
{
	speculate(var1);
	printf("This statement and the following are speculative!!\n");
	printf("Speculating on id: %d\n", var1);
	self->i++;
}

BEGIN
/(self->i)/
{
	discard(var1);
	self->discard++;
	commit(var1);
	self->commit++;
}

BEGIN
/self->commit/
{
	printf("Commited a discarded buffer\n");
	exit(0);
}


BEGIN
/!self->commit/
{
	printf("Couldnt commit a discarded buffer\n");
	exit(1);
}

ERROR
{
	exit(1);
}
