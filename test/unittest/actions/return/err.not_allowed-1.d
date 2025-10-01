/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: return() is not allowed for fbt probes
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

fbt:vmlinux:__*_sys_getpid:entry
/ok/
{
	return(0);
}
