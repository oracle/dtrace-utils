/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The substr() subroutine stores the correct substring when the
 *	      count is not specified, or greater or equal to the maximum string
 *	      size.
 *
 * SECTION: Actions and Subroutines/substr()
 */

#pragma D option quiet
#pragma D option rawbytes
#pragma D option strsize=13

BEGIN
{
	x = "1234567890123";
	trace(x);
	trace(substr(x, 0));
	trace(substr(x, 0, 12));
	trace(substr(x, 0, 13));
	trace(substr(x, 0, 14));
	exit(0);
}

ERROR
{
	exit(1);
}
