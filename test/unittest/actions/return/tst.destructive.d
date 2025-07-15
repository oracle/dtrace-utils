/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: return() is allowed when destructive execution is allowed
 *
 * SECTION: Actions and Subroutines/return()
 */

#pragma D option quiet
#pragma D option destructive

BEGIN
{
	ok = 0;
	exit(0);
}

rawfbt:btrfs:open_ctree:entry
/ok/
{
	return(0);
}
