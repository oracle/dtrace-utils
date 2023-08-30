/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Loading from dereferenced alloca pointers works.
 *
 * SECTION: Actions and Subroutines/alloca()
 */

#pragma D option quiet

int *val;

BEGIN
{
	val = alloca(sizeof(int));
	*val = 0x42;
	trace(*val);
	exit(0);
}

ERROR
{
	exit(1);
}
