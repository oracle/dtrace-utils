/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

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
	self->discardFlag = 0;
	self->var1 = speculation();
	printf("Speculative buffer ID: %d\n", self->var1);
	self->spec = speculation();
	printf("Speculative buffer ID: %d\n", self->spec);
}

BEGIN
/0 == self->spec/
{
	discard(self->spec);
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
