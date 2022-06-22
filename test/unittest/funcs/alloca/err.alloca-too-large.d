/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Allocation size is checked when passed from a variable.
 *
 * SECTION: Actions and Subroutines/alloca()
 */

#pragma D option quiet
#pragma D option scratchsize=64

BEGIN
{
	sz = 65;
	alloca(sz);
	exit(0);
}

ERROR
{
	exit(1);
}
