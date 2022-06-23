/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The -xundef option can be used to undefine macros (requires -C).
 *
 * SECTION: Options and Tunables/Consumer Options
 */

/* @@runtest-opts: -xundef=__linux -C */

BEGIN
{
#ifdef __linux
	exit(1);
#else
	exit(0);
#endif
}

ERROR
{
	exit(1);
}
