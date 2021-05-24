/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The 'caller' should be consistent with stack().
 *
 * SECTION: Variables/Built-in Variables/caller
 */

#pragma D option quiet
#pragma D option destructive

BEGIN
{
        system("echo write something > /dev/null");
}

fbt::ksys_write:entry
{
	stack(2);
	sym(caller);
	exit(0);
}

ERROR {
	exit(1);
}
