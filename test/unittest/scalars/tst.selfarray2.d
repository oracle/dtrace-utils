/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@xfail: dtv2 */
/* @@timeout: 15 */

#pragma D option quiet
#pragma D option destructive
#pragma D option dynvarsize=1m

struct bar {
	int pid;
	struct task_struct *curthread;
};

self struct bar foo[int];

syscall:::entry
/!self->foo[0].pid/
{
	self->foo[0].pid = pid;
	self->foo[0].curthread = curthread;
}

syscall:::entry
/self->foo[0].pid != pid/
{
	printf("expected %d, found %d (found curthread %p, curthread is %p)\n",
	    pid, self->foo[0].pid, self->foo[0].curthread, curthread);
	exit(1);
}

tick-100hz
{
	system("date > /dev/null")
}

tick-1sec
/i++ == 10/
{
	exit(0);
}
