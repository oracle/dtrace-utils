/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: bcopies to before the bottom of alloca()ed memory fail.
 *
 * SECTION: Actions and Subroutines/alloca()
 */

#pragma D option quiet

BEGIN
{
	a = "0";
	s = (char *)alloca(15);
	bcopy(a, s-1, 1);
        exit(0);
}

ERROR
{
	exit(1);
}
