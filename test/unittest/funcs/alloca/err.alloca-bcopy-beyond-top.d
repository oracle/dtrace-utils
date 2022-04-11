/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: bcopies to past the end of alloca()ed memory fail.
 *
 * SECTION: Actions and Subroutines/alloca()
 */

#pragma D option quiet

BEGIN
{
	a = "01";
	s = (char *)alloca(16);
	bcopy(a, &s[16], 1);
        exit(0);
}

ERROR
{
	exit(1);
}
