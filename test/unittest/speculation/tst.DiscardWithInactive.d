/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: When discard() is called with an inactive buffer number,
 * it is ignored.
 *
 * SECTION: Speculative Tracing/Discarding a Speculation
 */
#pragma D option quiet
BEGIN
{
	self->i = 0;
}

BEGIN
{
	discard(1);
}

BEGIN
{
	trace("This should be seen");
}

BEGIN
{
	exit(0);
}

ERROR
{
	exit(1);
}
