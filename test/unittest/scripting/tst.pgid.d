#!/usr/sbin/dtrace -qs

/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Script which prints effective process group id.
 *
 * SECTION: Scripting
 *
 */

BEGIN
{
	printf("The pgid is %d\n", $pgid);
	exit(0);
}
