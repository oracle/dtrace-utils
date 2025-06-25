/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* Check that 'stackdepth' is consistent with stack(). */

#pragma D option quiet

lockstat:::*acquire
/pid == $target/
{
	printf("DEPTH %d\n", stackdepth);
	printf("TRACE BEGIN\n");
	stack(100);
	printf("TRACE END\n");
	exit(0);
}

ERROR
{
	printf("error encountered\n");
	exit(1);
}
