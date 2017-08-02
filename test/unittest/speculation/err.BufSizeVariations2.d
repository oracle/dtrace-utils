/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Verify the behavior of variations in bufsize.
 *
 * SECTION: Speculative Tracing/Options and Tuning;
 *	    Options and Tunables/bufsize
 *
 * NOTES: This test behaves differently depending on the values
 * assigned to bufsize.
 * 1. 0 > bufsize.
 * 2. 0 == bufsize.
 * 3. 0 < bufsize <= 7
 * 4. 8 <= bufsize <= 31
 * 5. 32 <= bufsize <= 47
 * 6. 48 <= bufsize <= 71
 * 7. 72 <= bufsize
 */

#pragma D option quiet
#pragma D option bufsize=4

BEGIN
{
	self->speculateFlag = 0;
	self->commitFlag = 0;
	self->spec = speculation();
	printf("Speculative buffer ID: %d\n", self->spec);
}

BEGIN
{
	speculate(self->spec);
	printf("Lots of data\n");
	printf("Has to be crammed into this buffer\n");
	printf("Until it overflows\n");
	printf("And causes flops\n");
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
