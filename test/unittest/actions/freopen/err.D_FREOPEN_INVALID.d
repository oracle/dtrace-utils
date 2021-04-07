/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The freopen() action cannot have "." as first argument.
 *
 * SECTION: Actions and Subroutines/freopen()
 */

#pragma D option quiet

BEGIN
{
	freopen(".");
	exit(0);
}
