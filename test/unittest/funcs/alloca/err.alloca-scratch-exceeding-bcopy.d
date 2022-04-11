/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Exceeding the size of alloca()ed memory with a bcopy is an error.
 *
 * SECTION: Actions and Subroutines/alloca()
 */

/*
 * Intentionally use an unaligned size, to make sure that errors are still
 * emitted when accessing beyond the last byte when the size is not a
 * multiple of the max type size.
 */

#pragma D option quiet
#pragma D option scratchsize=9

string a;

BEGIN
{
	a = "0123456789abcdefgh";
	s = (char *)alloca(9);
	bcopy(a, s, 17);
	exit((s[0] == '0' && s[16] == 'g') ? 0 : 1);
}

ERROR
{
	exit(1);
}
