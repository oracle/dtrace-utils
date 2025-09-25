/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Test printf with %k and a stack argument.
 *
 * SECTION: Output Formatting/printf()
 */

#pragma D option destructive

BEGIN
{
	system("echo write something > /dev/null");
}

fbt::ksys_write:entry
{
	printf("%k", stack(1));
	printf("%k", stack(2));
	printf("%k", stack(3));
	printf("%k", stack());
	exit(0);
}

ERROR
{
	exit(1);
}
