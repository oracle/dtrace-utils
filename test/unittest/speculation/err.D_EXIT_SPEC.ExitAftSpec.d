/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 * Exit action may never be speculative.
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
	self->spec = speculation();
}

syscall:::
/self->spec/
{
	speculate(self->spec);
	i++;
	printf("i: %d\n", i);
	exit(0);
}

syscall:::
/1 == i/
{
	exit(0);
}

END
{
	exit(0);
}

ERROR
{
	exit(0);
}
