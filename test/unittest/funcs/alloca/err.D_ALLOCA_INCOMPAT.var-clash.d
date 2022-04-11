/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: the same variable cannot be reused for alloca and
 *            non-alloca pointers, even in different clauses.
 *            You can't fake it out by assigning NULL in between.
 *
 * SECTION: Actions and Subroutines/alloca()
 */

#pragma D option quiet

BEGIN
{
	a = (char *) alloca(1);
}

BEGIN
{
	b = (char *) &`max_pfn;
        a = NULL;
        a = b + 5;
	trace(a);
	exit(0);
}
