/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Can call discard on an already discarded buffer.
 *
 * SECTION: Speculative Tracing/Discarding a Speculation
 *
 */
#pragma D option quiet

BEGIN
{
	self->i = 0;
	self->discard1 = 0;
	self->discard2 = 0;
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
	self->discard1++;
	discard(var1);
	self->discard2++;
}

BEGIN
/(self->discard2) && (self->discard1)/
{
	printf("Discarded a discarded buffer\n");
	exit(0);
}


BEGIN
/(!self->discard2) || (!self->discard1)/
{
	printf("Couldn't discard a discarded buffer\n");
	exit(1);
}

ERROR
{
	exit(1);
}
