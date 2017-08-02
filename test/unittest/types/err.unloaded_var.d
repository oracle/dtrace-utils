/*
 * Oracle Linux DTrace.
 * Copyright © 2012, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *   Verify that we cannot reference a variable in a kernel module that is not
 *   loaded.
 *
 * SECTION: Structs and Unions/Structs;
 *	Variables/External Variables
 */

#pragma D option quiet

BEGIN
{
	t = hysdn`cardmax;
	exit(0);
}
