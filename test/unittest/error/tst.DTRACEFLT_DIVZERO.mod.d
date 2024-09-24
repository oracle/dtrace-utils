/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Test DTRACEFLT_DIVZERO error reporting for a modulo 0 operation.
 *
 * SECTION: dtrace Provider
 */

#pragma D option quiet

BEGIN { myid = id; } /* clause 0 */
BEGIN { myid = id; } /* clause 1 */
BEGIN { myid = id; } /* clause 2 */
BEGIN { myid = id; } /* clause 3 */

BEGIN
{
	myid = id;
	i = 1;
	j = 2;
	j = i % (j - 2);
	exit(1);
}

ERROR
{
	exit(arg1 != myid || arg2 != 4 || arg4 != DTRACEFLT_DIVZERO ||
	     arg5 != 0);
}
