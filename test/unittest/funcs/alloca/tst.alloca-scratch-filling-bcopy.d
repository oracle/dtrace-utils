/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: It is possible to store and load structures that fill up
 *            alloca()ed memory.
 *
 * SECTION: Actions and Subroutines/alloca()
 */

#pragma D option quiet
#pragma D option scratchsize=8

string a;

BEGIN
{
	a = "01234567";
	s = (char *)alloca(8);
	bcopy(a, s, 8);
	exit((s[0] == '0' && s[7] == '7') ? 0 : 1);
}

ERROR
{
	exit(1);
}
