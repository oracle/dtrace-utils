/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: bcopy to scratch space is allowed (even though documentation
 *	      claims that it is not)
 *
 * SECTION: Actions and Subroutines/alloca();
 * 	Actions and Subroutines/bcopy()
 *
 */

#pragma D option quiet

BEGIN
{
	ptr = (unsigned long *)alloca(sizeof(unsigned long));
	bcopy((void *)&`max_pfn, ptr, sizeof(unsigned long));
	ptr2 = (unsigned long *)alloca(sizeof(unsigned long));
	bcopy(ptr, ptr2, sizeof(unsigned long));
	exit(*ptr == *ptr2 ? 0 : 1);
}

ERROR
{
	exit(1);
}
