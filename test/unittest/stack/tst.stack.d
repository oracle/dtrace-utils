/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Basic sanity checks on stack() pass.
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
	stack(1);
	stack(2);
	stack(3);
	stack();
	exit(0);
}
