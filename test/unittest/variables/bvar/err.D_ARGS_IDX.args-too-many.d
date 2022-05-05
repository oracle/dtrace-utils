/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Accessing args[i] for probes with n arguments fails when i >= n.
 *
 * SECTION: Variables/Built-in Variables/args
 */

#pragma D option quiet
#pragma D option destructive

BEGIN
{
	system("echo write something > /dev/null");
}

write:entry {
	trace(args[0]);
	trace(args[1]);
	trace(args[2]);
	trace(args[3]);
	exit(0);
}

ERROR {
	exit(1);
}
