/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * speculate() may not follow data recording actions.
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
	printf("Entering syscall clause\n");
	speculate(self->spec);
	i++;
	printf("i: %d\n", i);
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
