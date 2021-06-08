/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The third argument to substr() should be a scalar.
 *
 * SECTION: Actions and Subroutines/substr()
 */

BEGIN
{
	trace(substr(probename, 1, "a"));
	exit(0);
}
