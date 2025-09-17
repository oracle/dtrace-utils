/*
 * Linux DTrace
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#!/usr/sbin/dtrace -s

/*
 *  SYNOPSIS
 *    sudo ./201built-in-variables-timestamps.d
 *
 *  DESCRIPTION
 *    Built-in variables provide timestamps:
 *
 *    - timestamp counts nanoseconds since
 *      some arbitrary point in the past.
 *      Use differences between values to measure
 *      elapsed time.  The value is captured only
 *      once per clause.
 *
 *    - walltimestamp counts nanoseconds since
 *      00:00 Universal Coordinated Time, January 1, 1970.
 */

dtrace:::BEGIN
{
	printf("\n%d %d\n", timestamp, walltimestamp);
	printf(  "%d %d\n", timestamp, walltimestamp);
}

dtrace:::BEGIN
{
	printf("\n%d %d\n", timestamp, walltimestamp);
	printf(  "%d %d\n", timestamp, walltimestamp);
	exit(0);
}
