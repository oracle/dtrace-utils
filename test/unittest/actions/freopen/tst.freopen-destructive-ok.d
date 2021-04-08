/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: freopen() is allowed when destructive execution is allowed
 *
 * SECTION: Actions and Subroutines/freopen()
 */

#pragma D option quiet
#pragma D option destructive

BEGIN
{
	freopen("/dev/null");
	exit(0);
}
