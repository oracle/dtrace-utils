/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@trigger: periodic_output */
/* @@nosort */

#pragma D option quiet
#pragma D option aggrate=1ms
#pragma D option switchrate=50ms

BEGIN
{
	n = 0;
	i = 0; @[i] = sum(10 - i);
	i = 1; @[i] = sum(10 - i);
	i = 2; @[i] = sum(10 - i);
	i = 3; @[i] = sum(10 - i);
	i = 4; @[i] = sum(10 - i);
	trunc(@);
}

/* Wait for the trunc() to be performed by the consumer. */
syscall::write:entry
/pid == $target && n++ == 2/
{
	i = 5; @[i] = sum(10 - i);
	i = 6; @[i] = sum(10 - i);
	i = 7; @[i] = sum(10 - i);
	i = 8; @[i] = sum(10 - i);
	i = 9; @[i] = sum(10 - i);
	exit(0);
}

END
{
	printa(@);
}
