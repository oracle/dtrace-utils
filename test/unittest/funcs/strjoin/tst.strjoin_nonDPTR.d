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
	/* "abcdef" */
	system("printf '\x61\x62\x63\x64\x65\x66' > /dev/null 2>&1");
}

syscall::write:entry
/ppid == $pid/
{
	printf("|%s|\n", strjoin((void *)arg1, (void *)arg1));
	exit(0);
}

ERROR { exit(1); }
