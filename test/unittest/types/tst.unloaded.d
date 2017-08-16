/*
 * Oracle Linux DTrace.
 * Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *   Verify that we can still reference a type in a kernel module that is not
 *   loaded.
 *
 * SECTION: Structs and Unions/Structs;
 *	Variables/External Variables
 */

#pragma D option quiet

BEGIN
{
	s = (struct can`dev_rcv_lists *)NULL;
	exit(0);
}
