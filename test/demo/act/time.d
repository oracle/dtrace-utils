/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: none */

#pragma D option quiet
#pragma D option destructive
#pragma D option switchrate=5sec

tick-1sec
/n++ < 5/
{
	printf("walltime  : %Y\n", walltimestamp);
	printf("date      : ");
	system("date +'%%Y %%b %%d  %%H:%%M:%%S'");
	printf("\n");
}

tick-1sec
/n == 5/
{
	exit(0);
}
