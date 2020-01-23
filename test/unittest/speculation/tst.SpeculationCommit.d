/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION: Test the normal behavior of speculate() and commit().
 *
 * SECTION: Speculative Tracing/Committing a Speculation;
 *	Actions and Subroutines/speculation();
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
	self->commit = 0;
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
	commit(self->var);
	self->commit++;
}

BEGIN
/(1 == self->commit)/
{
	printf("Succesfully tested buffer commit\n");
	exit(0);
}

BEGIN
/(0 == self->commit)/
{
	printf("Failed to commit buffer\n");
	exit(1);
}
