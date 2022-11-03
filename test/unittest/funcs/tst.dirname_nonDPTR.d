/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet
#pragma D option destructive

BEGIN
{
	/* "/foo/bar///baz/" */
	system("printf '\x2f\x66\x6f\x6f\x2f\x62\x61\x72\x2f\x2f\x2f\x62\x61\x7a\x2f' > /dev/null 2>&1");
}

syscall::write:entry
/ppid == $pid/
{
	printf("|%s|\n", dirname((void *)arg1));
	exit(0);
}

ERROR { exit(1); }
