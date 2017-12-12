/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: FBT provider entry probe args beyond 5th
 *
 * SECTION: FBT Provider/Probes
 */

/*
#pragma D option quiet
 */
#pragma D option statusrate=10ms
#pragma D option destructive

BEGIN
{
	me = pid;
	num_entry = 0;
	num_return = 0;
	fails = 0;
	system("ls >/dev/null");
}

fbt::dt_debug_probe:entry
{
	printf("args %d %d %d %d %d %d %d %d\n",
	       arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
}

sdt::dt_debug_probe:test
{
	printf("args %d %d %d %d %d %d %d %d\n",
	       arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
}

fbt::dt_debug_probe:entry
{
	num_entry++;
	self->arg0 = arg0;
	self->arg1 = arg1;
	self->arg2 = arg2;
	self->arg3 = arg3;
	self->arg4 = arg4;
	self->arg5 = arg5;
	self->arg6 = arg6;
	self->arg7 = arg7;
}

fbt::dt_debug_probe:return
/num_entry > 0/
{
	exit(0);
}
