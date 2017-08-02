/*
 * Oracle Linux DTrace.
 * Copyright © 2006, 2012, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	memory allocated by alloca() is only valid within the clause
 *	it is allocated.
 *
 * SECTION: Actions and Subroutines/alloca()
 *
 */

#pragma D option quiet


BEGIN
{
	ptr = alloca(sizeof (unsigned long));
}

tick-1
{
	bcopy((void *)&`max_pfn, ptr, sizeof (unsigned long));
	exit(0);
}

ERROR
{
	exit(1);
}
