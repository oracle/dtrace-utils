/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@skip: Solaris-specific, unconverted */

/* @@runtest-opts: /bin/date */

#pragma D option bufpolicy=ring
#pragma D option bufsize=16k

syscall:::entry
/execname == $1/
{
	trace(timestamp);
}

syscall::rexit:entry
{
	exit(0);
}
