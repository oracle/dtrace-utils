/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The substr() subroutine stores the correct string length prefix.
 *
 * SECTION: Actions and Subroutines/substr()
 */

#pragma D option quiet
#pragma D option rawbytes
#pragma D option strsize=13

BEGIN
{
	trace(strjoin((x = substr("abcdef", 2)), (y = substr("ABCDEF", 3))));
	trace(x);
	trace(y);
	exit(0);
}

ERROR
{
	exit(1);
}
