/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Exactly two Arguments are required for copyin().
 *
 * SECTION: Actions and Subroutines/copyin()
 *	    User Process Tracing/copyin() and copyinstr()
 */

BEGIN
{
	copyin(1, 2, 3);
	exit(1);
}

ERROR
{
	exit(0);
}
