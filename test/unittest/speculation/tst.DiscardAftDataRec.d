/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Discard may not follow data recording actions.
 *
 * SECTION: Speculative Tracing/Discarding a Speculation
 *
 */

#pragma D option quiet

BEGIN
{
	self->spec = speculation();
	printf("Speculative buffer ID: %d\n", self->spec);
}

BEGIN
{
	printf("Can have data recording before discarding\n");
	discard(self->spec);
	exit(0);
}
