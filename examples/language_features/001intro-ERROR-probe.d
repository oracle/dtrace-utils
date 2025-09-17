/*
 * Linux DTrace
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#!/usr/sbin/dtrace -s

/*
 *  SYNOPSIS
 *    sudo ./001intro-ERROR-probe.d
 *
 *  DESCRIPTION
 *    The ERROR probe can be used to handle run-time errors.
 *    If a clause encounters an error, its output is not seen
 *    and the ERROR probe is triggered.  Other clauses associated
 *    with the guilty probe, however, do execute.
 */

dtrace:::ERROR
{
	printf("An error occurred.\n");
}

dtrace:::BEGIN
{
	printf("WE SHOULD NOT SEE THIS!\n");
	printf("Dereference an illegal address: %d\n", *((int *)0x1234));
	printf("WE SHOULD NOT GET HERE!\n");
}

dtrace:::BEGIN
{
	printf("This is a different clause.\n");
	exit(0);
}
