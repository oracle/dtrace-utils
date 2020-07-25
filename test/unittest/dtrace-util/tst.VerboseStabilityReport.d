/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Using -v option with dtrace utility produces a program stability report
 * showing the minimum interface stability and dependency level for
 * the specified D programs.
 *
 * SECTION: dtrace Utility/-s Option;
 * 	dtrace Utility/-v Option
 *
 * NOTES: Use this file as
 * /usr/sbin/dtrace -vs man.VerboseStabilityReport.d
 *
 */

/* @@runtest-opts: -v */

BEGIN
{
	printf("This test should compile: %d\n", 2);
	exit(0);
}
