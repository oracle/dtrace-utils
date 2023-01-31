/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The ldpath option changes the ld path.
 *
 * SECTION: Options and Tunables/Consumer Options
 */

/* @@runtest-opts: -G -xldpath=/nonexistent/ld */

BEGIN
{
	exit(0);
}
