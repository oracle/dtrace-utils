/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	bcopy should not copy from one memory location to another
 *	if the copy size is very large.
 *
 * SECTION: Actions and Subroutines/alloca();
 * 	Actions and Subroutines/bcopy()
 *
 */

#pragma D option quiet
#pragma D option scratchsize=256


BEGIN
{
	ptr = alloca(20);

	/* Attempt to bcopy to scratch memory that isn't allocated,
	   with a max exceeding the verifier-checked bound of
	   2*scratchsize.  */
	bcopy((void *)&`max_pfn, ptr, 2048000);
	exit(0);
}

ERROR
{
	exit(1);
}
