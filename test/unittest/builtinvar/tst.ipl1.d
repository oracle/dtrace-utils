/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * To print ipl and make sure it succeeds
 *
 * SECTION: Variables/Built-in Variables
 */

#pragma D option quiet

BEGIN
{
	printf("The interrupt priority level = %u\n", ipl);
	exit (0);
}
