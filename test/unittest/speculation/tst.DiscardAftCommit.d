/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Can call discard() on a buffer after it has been commited.
 *
 * SECTION: Speculative Tracing/Discarding a Speculation;
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
	commit(var1);
	self->commit++;
	discard(var1);
	self->discard++;
}

BEGIN
/self->discard/
{
	printf("Discarded a commited buffer\n");
	exit(0);
}


BEGIN
/!self->discard/
{
	printf("Couldnt discard a commited buffer\n");
	exit(1);
}

ERROR
{
	exit(1);
}
