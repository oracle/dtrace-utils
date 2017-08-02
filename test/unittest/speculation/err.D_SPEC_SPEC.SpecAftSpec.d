/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * A clause can contain only one speculate() call.
 *
 * SECTION: Speculative Tracing/Using a Speculation
 *
 */
#pragma D option quiet

BEGIN
{
	self->i = 0;
	var1 = speculation();
	printf("Speculation ID: %d\n", var1);
	var2 = speculation();
	printf("Speculation ID: %d\n", var2);
}

profile:::tick-1sec
{
	speculate(var1);
	printf("Speculating on id: %d\n", var1);
	speculate(var2);
	printf("Speculating on id: %d", var2);
	self->i++;

}

profile:::tick-1sec
/1 > self->i/
{
	exit(0);
}

END
{
	exit(0);
}
