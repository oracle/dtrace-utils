/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Data recording actions may follow discard.
 *
 * SECTION: Speculative Tracing/Discarding a Speculation
 */
#pragma D option quiet

BEGIN
{
	self->speculateFlag = 0;
	self->discardFlag = 0;
	self->spec = speculation();
}

BEGIN
/self->spec/
{
	speculate(self->spec);
	printf("Called speculate with id: %d\n", self->spec);
	self->speculateFlag++;
}

BEGIN
/(self->spec) && (self->speculateFlag)/
{
	discard(self->spec);
	self->discardFlag++;
	printf("Data recording after discard\n");
}

BEGIN
/self->discardFlag/
{
	exit(0);
}

BEGIN
/!self->discardFlag/
{
	exit(1);
}

ERROR
{
	exit(1);
}
