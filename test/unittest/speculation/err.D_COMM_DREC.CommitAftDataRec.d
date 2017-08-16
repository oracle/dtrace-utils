/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Commit may not follow data recording actions.
 *
 * SECTION: Speculative Tracing/Committing a Speculation
 */

#pragma D option quiet

BEGIN
{
	self->speculateFlag = 0;
	self->spec = speculation();
	printf("Speculative buffer ID: %d\n", self->spec);
	exit(0);
}

END
{
	printf("This test shouldnt have compiled\n");
	commit(self->spec);
	exit(0);
}
