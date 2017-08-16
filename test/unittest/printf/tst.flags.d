/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Test all of the various formatting flags (except %' because that
 *  requires locale support).
 *
 * SECTION: Output Formatting/printf()
 *
 */

#pragma D option quiet

BEGIN
{
	printf("\n");
	printf("# %#8x\n", 0x123);
	printf("0 %08x\n", 0x123);
	printf("- %-8x\n", 0x123);
	printf("+ %+8d\n", 123);
	printf("  % 8d\n", 123);
	exit(0);
}
