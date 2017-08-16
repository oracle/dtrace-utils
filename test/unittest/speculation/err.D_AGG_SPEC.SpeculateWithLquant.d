/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Aggregating functions may never be speculative.
 *
 * SECTION: Speculative Tracing/Using a Speculation
 *
 */
#pragma D option quiet

BEGIN
{
	i = 0;
}

syscall:::entry
{
	self->ts = timestamp;
}

syscall:::return
/self->ts/
{
	var = speculation();
	speculate(var);
	printf("Speculation ID: %d", var);
	@Lqauntus[execname] = lquantize(timestamp - self->ts, 0, 100, 1);
	i++;
}

syscall:::
/1 == i/
{
	exit(0);
}

ERROR
{
	exit(0);
}

END
{
	exit(0);
}
