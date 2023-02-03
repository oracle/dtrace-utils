/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Arguments are evaluated in the correct order.
 *
 * SECTION: Actions and Subroutines/copyinto()
 */
/* @@trigger: delaydie */

#pragma D option quiet

syscall::write:entry
/pid == $target/
{
	ptr = (char *)alloca(16);

	/* initialize with '.' */
	ptr[ 0] = ptr[ 1] = ptr[ 2] = ptr[ 3] =
	ptr[ 4] = ptr[ 5] = ptr[ 6] = ptr[ 7] =
	ptr[ 8] = ptr[ 9] = ptr[10] = ptr[11] =
	ptr[12] = ptr[13] = ptr[14] = ptr[15] = '.';

	/* execute subroutine with order-dependent arguments */
	n = 2;
	copyinto(arg1 + (n++), (n++), (void*)(ptr + (n++)));

	/* print results */
	printf("%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n",
	    ptr[ 0], ptr[ 1], ptr[ 2], ptr[ 3],
	    ptr[ 4], ptr[ 5], ptr[ 6], ptr[ 7],
	    ptr[ 8], ptr[ 9], ptr[10], ptr[11],
	    ptr[12], ptr[13], ptr[14], ptr[15]);

	/* repeat all that but with order-independent arguments */
	ptr[ 0] = ptr[ 1] = ptr[ 2] = ptr[ 3] =
	ptr[ 4] = ptr[ 5] = ptr[ 6] = ptr[ 7] =
	ptr[ 8] = ptr[ 9] = ptr[10] = ptr[11] =
	ptr[12] = ptr[13] = ptr[14] = ptr[15] = '.';
	copyinto(arg1 + 2, 3, (void *)(ptr + 4));
	printf("%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n",
	    ptr[ 0], ptr[ 1], ptr[ 2], ptr[ 3],
	    ptr[ 4], ptr[ 5], ptr[ 6], ptr[ 7],
	    ptr[ 8], ptr[ 9], ptr[10], ptr[11],
	    ptr[12], ptr[13], ptr[14], ptr[15]);

	exit(0);
}

ERROR
{
	exit(1);
}
