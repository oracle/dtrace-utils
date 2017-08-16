/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Using the -CH option with dtrace invocation displays the list of
 * pathnames of included files one per line to the stderr.
 *
 * SECTION: dtrace Utility/-C Option;
 *	dtrace Utility/-H Option
 *
 * NOTES: Use this file as
 * /usr/sbin/dtrace -qCH -s man.IncludedFilePath.d
 *
 */

/* @@runtest-opts: -qCH */
/* @@xfail: __gnuc_va_list problem with stdio inclusion; needs postprocessor to mangle pathnames */

#include "stdio.h"

BEGIN
{
	printf("This test should compile.\n");
	exit(0);
}
