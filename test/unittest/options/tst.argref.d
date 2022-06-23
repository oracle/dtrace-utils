/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The -xargref option surpresses the 'extraneous argument' error.
 *
 * SECTION: Options and Tunables/Consumer Options
 */

/* @@runtest-opts: -xargref 1234 */

BEGIN
{
	exit(0);
}

ERROR
{
	exit(1);
}
