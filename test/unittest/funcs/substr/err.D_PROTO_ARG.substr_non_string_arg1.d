/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The first argument to substr() should be a string.
 *
 * SECTION: Actions and Subroutines/substr()
 */

BEGIN
{
	trace(substr(10, 1));
	exit(0);
}
