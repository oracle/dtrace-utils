/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Arguments are evaluated in the correct order.
 *
 * SECTION: Actions and Subroutines/bcopy()
 */

#pragma D option quiet

BEGIN
{
	src = (char *)alloca(8);
	dst = (char *)alloca(8);

	/* initialize src */
	src[ 0] = 'a'; src[ 1] = 'b'; src[ 2] = 'c'; src[ 3] = 'd';
	src[ 4] = 'e'; src[ 5] = 'f'; src[ 6] = 'g'; src[ 7] = 'h';

	/* initialize dst with '.' */
	dst[ 0] = dst[ 1] = dst[ 2] = dst[ 3] =
	dst[ 4] = dst[ 5] = dst[ 6] = dst[ 7] = '.';

	/* execute subroutine with order-dependent arguments */
	n = 2;
	bcopy((void*)(src + (n++)), (void*)(dst + (n++)), (n++));

	/* print results */
	printf("%c%c%c%c%c%c%c%c\n",
	    dst[ 0], dst[ 1], dst[ 2], dst[ 3],
	    dst[ 4], dst[ 5], dst[ 6], dst[ 7]);

	/* repeat all that but with order-independent arguments */
	dst[ 0] = dst[ 1] = dst[ 2] = dst[ 3] =
	dst[ 4] = dst[ 5] = dst[ 6] = dst[ 7] = '.';
	bcopy((void*)(src + 2), (void*)(dst + 3), 4);
	printf("%c%c%c%c%c%c%c%c\n",
	    dst[ 0], dst[ 1], dst[ 2], dst[ 3],
	    dst[ 4], dst[ 5], dst[ 6], dst[ 7]);

	exit(0);
}

ERROR
{
	exit(1);
}
