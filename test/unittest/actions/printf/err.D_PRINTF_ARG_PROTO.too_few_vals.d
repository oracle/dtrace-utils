/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The printf() action requires values for all conversions.
 *
 * SECTION: Actions and Subroutines/printf()
 */

#pragma D option quiet

BEGIN
{
	printf("%d %d %d %d %d", 1, 2, 3);
	exit(0);
}
