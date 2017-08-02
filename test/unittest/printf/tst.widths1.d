/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Try to print charecter wide width using '?'
 *
 * SECTION: Output Formatting/printf()
 *
 */

#pragma D option quiet

BEGIN
{
	printf("\n");
	x = 0;

	printf("%?d\n", x++);
	printf("%?d\n", x++);
	printf("%?d\n", x++);
	printf("%?d\n", x++);
	printf("%?d\n", x++);
	printf("%?d\n", x++);

	exit(0);
}
