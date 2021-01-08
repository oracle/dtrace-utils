/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Test DTRACEFLT_BADADDR error reporting.
 *
 * SECTION: dtrace Provider
 */

#pragma D option quiet

BEGIN
{
	myepid = epid;
	trace(((struct task_struct *)NULL)->pid);
	exit(1);
}

ERROR
{
	exit(arg1 != myepid || arg2 != 1 || arg3 != -1 ||
	     arg4 != DTRACEFLT_BADADDR || arg5 != 0);
}
