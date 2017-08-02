/*
 * Oracle Linux DTrace.
 * Copyright © 2005, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * @@xfail: needs a trigger
 */

#pragma D option quiet

dtrace:::BEGIN
{
	start = timestamp;
}

sched:::on-cpu
/execname == $1/
{
	self->ts = timestamp;
}

sched:::off-cpu
/self->ts/
{
	@[cpu] = sum(timestamp - self->ts);
	self->ts = 0;
}

profile:::tick-1sec
/++x == 10/
{
	exit(0);
}
        
dtrace:::END
{
	printf("CPU distribution over %d seconds:\n\n",
	    (timestamp - start) / 1000000000);
	printf("CPU microseconds\n--- ------------\n");
	normalize(@, 1000);
	printa("%3d %@d\n", @);
}
