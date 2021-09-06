/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Verify the behavior of speculations with changes in specsize.
 *
 * SECTION: Speculative Tracing/Options and Tuning;
 *	Options and Tunables/specsize
 *
 */

#pragma D option quiet
#pragma D option specsize=40

long long x;

BEGIN
{
	x = 123456789;
	self->speculateFlag = 0;
	self->commitFlag = 0;
	self->spec = speculation();
	printf("Speculative buffer ID: %d\n", self->spec);
}

BEGIN
{
	speculate(self->spec);
	printf("%lld: Lots of data\n", x);
	printf("%lld: Has to be crammed into this buffer\n", x);
	printf("%lld: Until it overflows\n", x);
	printf("%lld: And causes flops\n", x);
	self->speculateFlag++;

}

BEGIN
/1 <= self->speculateFlag/
{
	commit(self->spec);
	self->commitFlag++;
}

BEGIN
/1 <= self->commitFlag/
{
	printf("Statement was executed\n");
	exit(0);
}

BEGIN
/1 > self->commitFlag/
{
	printf("Statement wasn't executed\n");
	exit(1);
}
