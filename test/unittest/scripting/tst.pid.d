#!/usr/sbin/dtrace -qs

/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Script which prints pid.
 *
 * SECTION: Scripting
 *
 */

BEGIN
{
	printf("The pid is %d\n", $pid);
	exit(0);
}
