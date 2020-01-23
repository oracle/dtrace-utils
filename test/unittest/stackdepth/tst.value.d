/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

#pragma D option quiet

/*
 * ASSERTION:
 *   Test the stackdepth variable.  Verify its value against the stack trace.
 *
 * SECTION: Variables/Built-in Variables
 *
 */

BEGIN
{
	printf("DEPTH %d\n", stackdepth);
	printf("TRACE BEGIN");
	stack();
	printf("TRACE END");
	exit(0);
}
