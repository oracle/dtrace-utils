/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* Check 'stackdepth'. */

#pragma D option quiet

BEGIN {
	printf("DEPTH %d\n", stackdepth);
	printf("TRACE BEGIN\n");
	stack();
	printf("TRACE END\n");
	exit(0);
}

ERROR
{
	printf("error encountered\n");
	exit(1);
}
