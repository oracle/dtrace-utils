/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/*
 * @@trigger: periodic_output
 */
/* Check that 'caller' is consistent with stack(). */

#pragma D option quiet

rawtp:sched::
{
	stack(2);
	sym(caller);
	exit(0);
}

ERROR
{
	printf("error encountered\n");
	exit(1);
}
