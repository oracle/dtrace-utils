/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet
#pragma D option destructive

BEGIN {
	/* baseline time stamp from system("date") */
	system("date +'%%s.%%N'");

	/* get five consecutive time stamps from DTrace */
	t1 = walltimestamp;
	t2 = walltimestamp;
	t3 = walltimestamp;
	t4 = walltimestamp;
	t5 = walltimestamp;

	/* report the first time stamp and the subsequent deltas */
	printf("%d\n", t1);
	printf("%d\n", t2 - t1);
	printf("%d\n", t3 - t2);
	printf("%d\n", t4 - t3);
	printf("%d\n", t5 - t4);
	exit(0);
}
