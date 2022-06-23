/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The -w option enables allowing destructive operations.
 *
 * SECTION: Options and Tunables/Consumer Options
 */

/* @@runtest-opts: -w */

BEGIN
{
	system("echo ok");
	exit(0);
}
