/*
 * Oracle Linux DTrace.
 * Copyright © 2006, 2012, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	bcopy should not allow a copy to memory that is not scratch memory.
 *
 * SECTION: Actions and Subroutines/alloca();
 * 	Actions and Subroutines/bcopy()
 *
 */

#pragma D option quiet


BEGIN
{
	ptr = alloca(sizeof (unsigned long));

	/* Attempt a copy from scratch memory to a kernel address */

	bcopy(ptr, (void *)&`max_pfn, sizeof (unsigned long));
	exit(0);
}

ERROR
{
	exit(1);
}
