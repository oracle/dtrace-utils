#!/usr/sbin/dtrace -qs

/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Attempt to print invalid arguments.
 *
 * SECTION: Scripting
 *
 */

BEGIN
{
	print("The arguments are %d %d\n", $-1);
	exit(0);
}
