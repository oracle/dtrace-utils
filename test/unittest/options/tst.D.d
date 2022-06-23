/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The -D option can be used to define macros (requires -C).
 *
 * SECTION: Options and Tunables/Consumer Options
 */

/* @@runtest-opts: -Dprobe=BEGIN -C */

probe
{
	exit(0);
}

ERROR
{
	exit(1);
}
