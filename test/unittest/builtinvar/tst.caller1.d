/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: To print caller from profile and make sure it succeeds.
 *
 * SECTION: Variables/Built-in Variables
 */

#pragma D option quiet

BEGIN
{
}

tick-10ms
{
	printf("The caller is %u\n", caller);
	exit (0);
}

ERROR
{
	exit(1);
}
