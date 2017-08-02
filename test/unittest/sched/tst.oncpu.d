/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@timeout: 15 */

#pragma D option switchrate=100hz
#pragma D option destructive

sched:::on-cpu
/pid == $pid/
{
	self->on++;
}

sched:::off-cpu
/pid == $pid && self->on/
{
	self->off++;
}

sched:::off-cpu
/self->on > 50 && self->off > 50/
{
	exit(0);
}

profile:::tick-1sec
/n++ > 10/
{
	exit(1);
}
