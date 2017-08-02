/*
 * Oracle Linux DTrace.
 * Copyright © 2015, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option destructive
#pragma D option quiet

/*
 * Ensure that tracing sigaltstack syscalls works.
 */
BEGIN
{
	/* Timeout after 3 seconds */
	timeout = timestamp + 3000000000;
	system("vi -c :q tmpfile >/dev/null 2>&1 &");
}

syscall::sigaltstack:entry
{
	exit(0);
}

profile:::tick-100ms
/timestamp > timeout/
{
	trace("test timed out");
	exit(1);
}
