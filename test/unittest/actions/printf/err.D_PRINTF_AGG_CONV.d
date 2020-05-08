/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The printf() action requires an aggregation value for conversions
 *	      with the '@' specifier.
 *
 * SECTION: Actions and Subroutines/printf()
 */

#pragma D option quiet

BEGIN
{
	printf("%@d", 1);
	exit(0);
}
