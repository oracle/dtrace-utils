/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The 'probefunc' variable returns the correct value.
 *
 * SECTION: Variables/Built-in Variables/probefunc
 */

#pragma D option quiet
#pragma D option destructive

BEGIN
{
	system("sleep 1s");
}

syscall::*sleep:entry
{
	trace(probefunc);
}

syscall::*sleep:entry
/probefunc == "clock_nanosleep"/
{
	exit(0);
}

syscall::*sleep:entry
/probefunc == "nanosleep"/
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
