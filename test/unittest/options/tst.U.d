/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The -U option can be used to undefine macros (requires -C).
 *
 * SECTION: Options and Tunables/Consumer Options
 */

/* @@runtest-opts: -U__linux -C */

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
