/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: It is possible to store to and load from the top byte of
 *	      alloca()'d memory.
 *
 * SECTION: Actions and Subroutines/alloca()
 */

#pragma D option quiet

BEGIN
{
	s = (char *)alloca(10);
	s[9] = 65;
	exit(s[9] == 65 ? 0 : 1);
}

ERROR
{
	exit(1);
}
