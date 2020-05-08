/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The printf() action requires an integer value for the width and
 *	      precision specifiers.  The width specifier is checked first.
 *
 * SECTION: Actions and Subroutines/printf()
 */

#pragma D option quiet

BEGIN
{
	printf("%*.*d", "", "", 1);
	exit(0);
}
