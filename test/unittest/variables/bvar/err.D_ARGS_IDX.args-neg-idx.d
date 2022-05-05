/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Accessing args[-1] for probes with arguments triggers a fault.
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
	trace(args[-1]);
	exit(0);
}

ERROR {
	exit(1);
}
