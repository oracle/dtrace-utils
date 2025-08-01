/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Without -w or -xdestructive, destructive operations are not ok,
 *            even if a clause will be ignored since it does not exist and
 *            -Z was specified.
 *
 * SECTION: Options and Tunables/Consumer Options
 */
/* @@runtest-opts: -Z */

BEGIN
{
	exit(0);
}

bogus:bogus:bogus:bogus
{
	system("echo ok");
}
