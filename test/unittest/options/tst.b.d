/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The -b option sets the trace buffer size.
 *
 * SECTION: Options and Tunables/Consumer Options
 */

/*
 * We use a buffer size that is just large enough to store the data recorded by
 * this script:
 *	- perf_event_header (8 bytes)
 *	- size (4 bytes)
 *	- specid (4 bytes)
 *	- PRID (4 bytes)
 *	- STID (4 bytes)
 *	- arg1 through arg5 (5 x 8 bytes)
 * So, 64 bytes.
 */
/* @@runtest-opts: -b64 */

BEGIN
{
	exit(0);
}

ERROR
{
	exit(1);
}
