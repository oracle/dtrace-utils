/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * The speculation() function returns a speculative identifier when a
 * speculative buffer is available.
 *
 * SECTION: Speculative Tracing/Creating a Speculation
 *
 */
#pragma D option quiet

BEGIN
{
	var = speculation();
	printf("Speculation ID: %d", var);
	exit(0);
}

END
/0 == var/
{
	exit(1);
}
