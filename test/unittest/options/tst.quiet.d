/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The -xquiet option can be used to output only explicitly traced
 *	      data.
 *
 * SECTION: Options and Tunables/Consumer Options
 */

/* @@runtest-opts: -xquiet */

BEGIN
{
	exit(0);
}
