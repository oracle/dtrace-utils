/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * An identifier returned from speculation must be passed to the speculate()
 * function.
 *
 * SECTION: Speculative Tracing/Creating a Speculation
 *
 */
#pragma D option quiet

BEGIN
{
	var = speculation();
	printf("Speculation ID: %d", var);
	self->i = 0;
}

profile:::tick-1sec
{
	speculate();
	self->i++;
}

profile:::tick-1sec
/1 <= self->i/
{
	exit(0);
}

END
{
	exit(0);
}
