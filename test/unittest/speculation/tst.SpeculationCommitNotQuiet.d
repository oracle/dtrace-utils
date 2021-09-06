/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Test the normal behavior of speculate() and commit() with
 * quiet mode turned off.
 *
 * SECTION: Speculative Tracing/Committing a Speculation;
 *	Actions and Subroutines/speculation()
 *
 */

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
