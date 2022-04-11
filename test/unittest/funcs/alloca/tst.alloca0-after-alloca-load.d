/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Calling alloca(0) is valid and subsequent calls will give the
 *	      same result, after a previous non-zero alloca, even when one
 *	      alloca(0) is stored to and loaded from a variable.
 *
 * SECTION: Actions and Subroutines/alloca()
 */

#pragma D option quiet

BEGIN
{
	s = alloca(10);
	s = alloca(0);
	exit(alloca(0) == s ? 0 : 1);
}

ERROR
{
	exit(1);
}
