/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

#pragma D option switchrate=100hz
#pragma D option destructive

sched:::enqueue
/pid == 0 && args[1]->pr_pid == $pid/
{
	self->one = 1;
}

sched:::enqueue
/self->one && args[2]->cpu_id >= 0 && args[2]->cpu_id <= `nr_cpu_ids/
{
	self->two = 1;
}

sched:::enqueue
/self->two && args[0]->pr_lwpid > 0/
{
	exit(0);
}
