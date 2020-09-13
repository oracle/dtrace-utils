/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Data recording actions may not follow commit.
 *
 * SECTION: Speculative Tracing/Committing a Speculation;
 *	Options and Tunables/cleanrate
 *
 */
#pragma D option quiet
#pragma D option cleanrate=2000hz

BEGIN
{
	self->speculateFlag = 0;
}

syscall:::entry
{
	self->spec = speculation();
}

syscall:::
/self->spec/
{
	speculate(self->spec);
	printf("Called speculate with id: %d\n", self->spec);
	self->speculateFlag++;
}

syscall:::
/(self->spec) && (self->speculateFlag)/
{
	commit(self->spec);
	printf("Data recording after commit\n");
}

END
{
	exit(0);
}

ERROR
{
	exit(0);
}
