/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Test the normal behavior of speculate() and discard().
 *
 * SECTION: Speculative Tracing/Discarding a Speculation;
 *	Options and Tunables/cleanrate
 *
 */
#pragma D option quiet
#pragma D option cleanrate=2000hz

BEGIN
{
	self->var = speculation();
	printf("Speculation ID: %d\n", self->var);
	self->speculate = 0;
	self->discard = 0;
}

BEGIN
/1 > self->speculate/
{
	speculate(self->var);
	self->speculate++;
	printf("Called speculate on id: %d\n", self->var);
}

BEGIN
/1 <= self->speculate/
{
	discard(self->var);
	self->discard++;
}

BEGIN
/(1 == self->discard)/
{
	printf("Succesfully tested buffer discard\n");
	exit(0);
}

BEGIN
/(0 == self->discard)/
{
	printf("Failed to discard buffer\n");
	exit(1);
}
