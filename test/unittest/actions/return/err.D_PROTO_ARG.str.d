/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The return() action cannot be passed a string argument.
 *
 * SECTION: Actions and Subroutines/return()
 */

#pragma D option quiet

BEGIN
{
	return("");
}
