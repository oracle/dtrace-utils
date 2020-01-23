/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 * The -I option can be used to search path for #include files when used
 * in conjunction with the -C option. The specified directory is inserted into
 * the search path adhead of the default directory list.
 *
 * SECTION: dtrace Utility/-C Option;
 * 	dtrace Utility/-I Option
 */

/* @@runtest-opts: -C -I test/utils */

#pragma D option quiet

#include "include-test.d"

BEGIN
{
	printf("Value of VALUE: %d\n", VALUE);
	exit(0);
}
