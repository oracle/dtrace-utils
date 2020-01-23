#!/usr/sbin/dtrace -s

/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 * 	print argument 0 I mean "$0"
 *
 * SECTION: Scripting
 *
 */

BEGIN
{
	printf("The arg0 is %s\n", $0);
	exit(0);
}
