/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * Check for a bug in which tstrings and stackdepth scratch overlapped.
 */

#pragma D option quiet

BEGIN
{
	printf("stack depth is %d\n", stackdepth);
	i = stackdepth;
	printf("substring is |%s|\n", substr(strjoin("abcdefghijkl", "mnopqrstuvwxyz"), i));
	printf("substring is |%s|\n", substr(strjoin("abcdefghijkl", "mnopqrstuvwxyz"), stackdepth));
	exit(0);
}
