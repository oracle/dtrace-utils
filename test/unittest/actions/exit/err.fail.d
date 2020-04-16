/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Calling exit(n) indicates failures if n is not 0.
 *
 * SECTION: Actions and Subroutines/exit()
 */

#pragma D option quiet

BEGIN
{
	exit(1);
}
