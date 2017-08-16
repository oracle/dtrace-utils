/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2012, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Test printf() with a simple string argument.
 *
 * SECTION: Output Formatting/printf()
 *
 */

#pragma D option quiet

BEGIN
{
	printf("symbol = %a", &`max_pfn);
	exit(0);
}
