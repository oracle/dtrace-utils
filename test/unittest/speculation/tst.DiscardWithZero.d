/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: An Id of zero though invalid may be passed to speculate(),
 * commit() and discard() without any ill effects.
 *
 * SECTION: Speculative Tracing/Creating a Speculation
 */
#pragma D option quiet

BEGIN
{
	self->discardFlag = 0;
}

BEGIN
{
	discard(0);
	self->discardFlag++;
}

BEGIN
/0 < self->discardFlag/
{
	printf("discard(), self->discardFlag = %d\n", self->discardFlag);
	exit(0);
}

BEGIN
/0 == self->discardFlag/
{
	printf("discard(), self->discardFlag = %d\n", self->discardFlag);
	exit(1);
}
