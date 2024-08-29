/*
 * Oracle Linux DTrace.
 * Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The -xbufsize option sets the trace buffer size.
 *
 * SECTION: Options and Tunables/Consumer Options
 */

/* @@runtest-opts: -xbufsize=-1 */

BEGIN
{
	exit(0);
}

ERROR
{
	exit(1);
}
