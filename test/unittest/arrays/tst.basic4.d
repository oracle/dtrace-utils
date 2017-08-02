/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Simple array test
 *
 * SECTION: Pointers and Arrays/Array Declarations and Storage
 *
 */


#pragma D option quiet

BEGIN
{
	a["test"] = 0;
}

tick-1
/a["test"] == 0/
{
	exit(0);
}

tick-1
/a["test"] != 0/
{
	printf("Expected 0, got %d\n", a["test"]);
	exit(1);
}
