/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: You can copy a string into an alloca'ed region and read
 *	      it out again.
 *
 * SECTION: Actions and Subroutines/alloca()
 */

#pragma D option quiet
#pragma D option scratchsize=512

BEGIN
{
	x = (string *)alloca(sizeof(string) + 1);
	*x = "abc";
	trace(*x);
	exit(0);
}

ERROR
{
	exit(1);
}
