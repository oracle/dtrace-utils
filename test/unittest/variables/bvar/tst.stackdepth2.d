/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option destructive
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
	system("echo write something > /dev/null");
}

fbt::ksys_write:entry
{
	printf("DEPTH %d\n", stackdepth);
	printf("TRACE BEGIN\n");
	stack();
	printf("TRACE END\n");
	exit(0);
}

ERROR
{
	exit(1);
}
