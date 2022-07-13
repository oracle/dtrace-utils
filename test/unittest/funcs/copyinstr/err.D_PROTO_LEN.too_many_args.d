/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: No more than two arguments can be passed for copyinstr().
 *
 * SECTION: Actions and Subroutines/copyinstr()
 *	    User Process Tracing/copyin() and copyinstr()
 */

BEGIN
{
	copyinstr(1, 2, 3);
	exit(1);
}

ERROR
{
	exit(0);
}
