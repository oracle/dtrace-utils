/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Simple printf() test.
 *
 * SECTION: Output Formatting/printf()
 *
 */


#pragma D option quiet

BEGIN
{
	printf("hello");
	exit(0);
}
