#!/usr/sbin/dtrace -s

/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: print script argument 0, i.e. "$0"
 *
 * SECTION: Scripting
 *
 */

BEGIN
{
	printf("The arg0 is %s\n", $0);
	exit(0);
}
