/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 *   Positive test for ring buffer policy.
 *
 * SECTION: Buffers and Buffering/ring Policy;
 *	Buffers and Buffering/Buffer Sizes;
 *	Options and Tunables/bufsize;
 *	Options and Tunables/bufpolicy
 */

/*
 * We make some regrettable assumptions about the implementation in this test.
 * First, we assume that each entry for the printf() of an int takes _exactly_
 * eight bytes (four bytes for the EPID, four bytes for the payload).  Second,
 * we assume that by allocating storage for n + 1 records, we will get exactly
 * n.  Here is why:  the final predicate that evaluates to false will reserve
 * space that it won't use.  This act of reservation will advance the wrapped
 * offset.  That record won't be subsequently used, but the wrapped offset has
 * advanced.  (And in this case, that old record is clobbered by the exit()
 * anyway.)  Thirdly:  we rely on t_cpu/cpu_id.  Finally:  we rely on being
 * able to run on the CPU that we first ran on.
 */
#pragma D option bufpolicy=ring
#pragma D option bufsize=40
#pragma D option quiet

int n;

BEGIN
{
	cpuid = -1;
}

tick-10msec
/cpuid == -1/
{
	cpuid = curcpu->cpu_id;
}

tick-10msec
/curcpu->cpu_id == cpuid && n < 100/
{
	printf("%d\n", n++);
}

tick-10msec
/n == 100/
{
	exit(0);
}
