/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The 'probemod' variable returns the correct value.
 *
 * SECTION: Variables/Built-in Variables/probemod
 */

#pragma D option quiet
#pragma D option destructive

BEGIN
{
	system("sleep 1s");
}

syscall::*sleep:entry
{
	trace(probemod);
}

syscall::*sleep:entry
/probemod == "vmlinux"/
{
	exit(0);
}

syscall::*sleep:entry
{
	exit(1);
}

ERROR {
	exit(1);
}
