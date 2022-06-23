/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: With no -xargref option, extraneous argument(s) cause an error.
 *
 * SECTION: Options and Tunables/Consumer Options
 */

/* @@runtest-opts: 1234 */

BEGIN
{
	exit(0);
}

ERROR
{
	exit(1);
}
