/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* Check that 'caller' is consistent with stack(). */

#pragma D option quiet

profile:::tick-50ms
/stackdepth > 1/
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
