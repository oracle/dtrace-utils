/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: alloca() allocations are suitably aligned for any native datatype.
 *
 * SECTION: Actions and Subroutines/alloca()
 */

#pragma D option quiet

BEGIN
{
	a = alloca(1);
	b = alloca(1);
	trace((uint64_t) b);
        trace("\n");
	trace((uint64_t) a);
        trace("\n");
	trace((uint64_t) b - (uint64_t) a);
        trace("\n");
	trace(sizeof (uint64_t));
        trace("\n");
	exit((uint64_t) b - (uint64_t) a != sizeof (uint64_t));
}

ERROR
{
	exit(1);
}
