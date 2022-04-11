/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: the same variable cannot be reused for alloca and
 *            non-alloca pointers, even in different clauses.
 *
 * SECTION: Actions and Subroutines/alloca()
 */

#pragma D option quiet

BEGIN
{
	b = (char *) &`max_pfn;
        a = b + 5;
}

BEGIN
{
	a = (char *) alloca(1);
	trace(a);
	exit(0);
}
