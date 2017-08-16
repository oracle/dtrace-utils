/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Setting the specsize option to an illegal value throws the compiler error
 * D_PRAGMA_OPTSET.
 *
 * SECTION: Speculative Tracing/Options and Tuning;
 *	Options and Tunables/specsize
 */

#pragma D option quiet
#pragma D option specsize=1b

BEGIN
{
	self->speculateFlag = 0;
	self->spec = speculation();
	printf("Speculative buffer ID: %d\n", self->spec);
	exit(0);
}

END
{
	printf("This shouldnt have compiled\n");
	exit(0);
}
