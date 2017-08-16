/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	copyout() should handle invalid args.
 *
 * SECTION: Actions and Subroutines/copyout()
 *
 */

#pragma D option destructive

BEGIN
{
	i = 3;
	copyout((void *)i, 0, 5);
	exit(0);
}

ERROR
{
	exit(1);
}
