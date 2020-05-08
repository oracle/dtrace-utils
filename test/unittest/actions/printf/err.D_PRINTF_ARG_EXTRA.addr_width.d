/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The printf() action provides a fixed field width for formatting
 *	      addresses in hexadecimal format, so providing a field width with
 *	      the '?' size specifier is not allowed.
 *
 * SECTION: Actions and Subroutines/printf()
 */

#pragma D option quiet

BEGIN
{
	printf("%?d\n", 1, 2);
	exit(0);
}
