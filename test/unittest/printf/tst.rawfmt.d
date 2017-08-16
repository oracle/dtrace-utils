/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Test printf() with a fixed string and no actual tracing arguments.
 *
 * SECTION: Output Formatting/printf()
 *
 */

#pragma D option quiet

BEGIN
{
	printf("hello world from BEGIN");
	exit(0);
}
