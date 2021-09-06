/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Calling speculate() with zero does not have any ill effects.
 * Statement after speculate does not execute.
 *
 * SECTION: Speculative Tracing/Creating a Speculation
 */

#pragma D option quiet

BEGIN
{
	self->speculateFlag = 0;
}

BEGIN
{
	speculate(0);
	self->speculateFlag++;
}

BEGIN
/1 == self->speculateFlag/
{
	printf("Statement was executed\n");
	exit(1);
}

BEGIN
/1 != self->speculateFlag/
{
	printf("Statement wasn't executed\n");
	exit(0);
}
