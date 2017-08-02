/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@xfail: needs trigger */

#pragma D option quiet

profile-97
/pid == $1/
{
	@proc[pid, execname] = count();
}

END
{
	printf("%-8s %-40s %s\n", "PID", "CMD", "COUNT");
	printa("%-8d %-40s %@d\n", @proc);
}
