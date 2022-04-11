/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Too-large alloca() allocations are a compile-time error.
 *
 * SECTION: Actions and Subroutines/alloca()
 *
 */

#pragma D option quiet

#pragma D option scratchsize=1024

BEGIN
{
	ptr = alloca(1025);
	exit(1);
}
