/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

int schedules;
int executes;

/* @@xfail: Solaris-specific */

/*
 * This script is a bit naughty:  it's assuming the implementation of the
 * VM system's page scanning.  If this implementation changes -- either by
 * changing the function that scans pages or by making that scanning
 * multithreaded -- this script will break.
 */
fbt::timeout:entry
/args[0] == (void *)&genunix`schedpaging/
{
	schedules++;
}

fbt::schedpaging:entry
/executes == 10/
{
	printf("%d schedules, %d executes\n", schedules, executes);
	exit(executes == schedules ? 0 : 1);
}

fbt::schedpaging:entry
{
	executes++;
}

