/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: not yet ported */

/*
 * ASSERTION: test for assertion failure in the ring buffer code
 *
 * SECTION: Buffers and Buffering/ring Policy; Misc
 */

/*
 * A script from Roch Bourbonnais that induced an assertion failure in the
 * ring buffer code.
 */
#pragma D option strsize=16
#pragma D option bufsize=10K
#pragma D option bufpolicy=ring

fbt:::entry
/(self->done == 0) && (curthread->t_cpu->cpu_intr_actv == 0) /
{
	self->done = 1;
	printf(" %u 0x%llX %d %d comm:%s csathr:%lld", timestamp,
	    (long long)curthread, pid, tid,
	    execname, (long long)stackdepth);
	stack(20);
}

fbt:::return
/(self->done == 0) && (curthread->t_cpu->cpu_intr_actv == 0) /
{
	self->done = 1;
	printf(" %u 0x%llX %d %d comm:%s csathr:%lld", timestamp,
	    (long long) curthread, pid, tid,
	    execname, (long long) stackdepth);
	stack(20);
}

fbt:::entry
{
	printf(" %u 0x%llX %d %d ", timestamp,
	    (long long)curthread, pid, tid);
}

fbt:::return
{
	printf(" %u 0x%llX %d %d tag:%d off:%d ", timestamp,
	    (long long)curthread, pid, tid, (int)arg1, (int)arg0);
}

mutex_enter:adaptive-acquire
{
	printf(" %u 0x%llX %d %d lock:0x%llX", timestamp,
	    (long long)curthread, pid, tid, arg0);
}

mutex_exit:adaptive-release
{
	printf(" %u 0x%llX %d %d lock:0x%llX", timestamp,
	    (long long) curthread, pid, tid, arg0);
}

tick-1sec
/n++ == 10/
{
	exit(0);
}
