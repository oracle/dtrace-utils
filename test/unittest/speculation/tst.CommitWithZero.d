/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: An Id of zero though invalid may be passed to speculate(),
 * commit() and discard() without any ill effects.
 *
 * SECTION: Speculative Tracing/Creating a Speculation;
 *	Options and Tunables/cleanrate
 */
#pragma D option quiet
#pragma D option cleanrate=4000hz

BEGIN
{
	self->commitFlag = 0;
	self->var1 = speculation();
	printf("Speculative buffer ID: %d\n", self->var1);
	self->spec = speculation();
	printf("Speculative buffer ID: %d\n", self->spec);
}

BEGIN
{
	commit(self->spec);
	self->commitFlag++;
}

BEGIN
/0 < self->commitFlag/
{
	printf("commit(), self->commitFlag = %d\n", self->commitFlag);
	exit(0);
}

BEGIN
/0 == self->commitFlag/
{
	printf("commit(), self->commitFlag = %d\n", self->commitFlag);
	exit(1);
}
