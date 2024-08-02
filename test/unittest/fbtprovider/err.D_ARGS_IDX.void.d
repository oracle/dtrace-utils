/*
 * Oracle Linux DTrace.
 * Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

/*
 * This should fail compilation due to accessing args[1] (not args[0]) for a
 * void function with arguments.
 */
fbt:vmlinux:exit_creds:return
{
	trace(args[0]);
	trace(args[1]);
	exit(0);
}

ERROR
{
	exit(1);
}
