/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: alloca does not live beyond clause boundaries; allocas in
 *	      subsequent clauses reuse the same addresses.
 *
 * SECTION: Actions and Subroutines/alloca()
 */

#pragma D option quiet

BEGIN
{
	s = (char *)alloca(1);
        *s = 'a';
}

BEGIN
{
	s2 = (char *)alloca(1);
        *s2 = 'b';
        exit(s == s2 ? 0 : 1);
}

ERROR
{
	exit(1);
}