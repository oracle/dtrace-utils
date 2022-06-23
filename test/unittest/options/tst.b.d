/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The -b option sets the trace buffer size.
 *
 * SECTION: Options and Tunables/Consumer Options
 */

/*
 * We use a buffer size of 60 because that should be the exact size necessary
 * to hold the trace records generated in this script:
 *	- perf_event_header (40 bytes)
 *	- size (4 bytes)
 *	- gap (4 bytes)
 *	- EPID (4 bytes)
 *	- tag (4 bytes)
 *	- exit value (4 bytes)
 */
/* @@runtest-opts: -b60 */

BEGIN
{
	exit(0);
}

ERROR
{
	exit(1);
}
