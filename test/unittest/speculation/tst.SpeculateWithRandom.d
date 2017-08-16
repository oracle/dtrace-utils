/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * When speculate() is called with an inactive buffer number, it is
 * ignored.
 *
 * SECTION: Speculative Tracing/Creating a Speculation
 *
 */
#pragma D option quiet
BEGIN
{
	self->i = 0;
}

BEGIN
{
	speculate(3456710);
	self->i++;
	printf("self->i: %d\n", self->i);

}

BEGIN
{
	exit(0);
}

ERROR
{
	exit(1);
}
