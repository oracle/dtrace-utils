#!/usr/sbin/dtrace -qs

/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *
 *	Try to call providers using macro variables.
 *
 * SECTION: Scripting
 *
 */

BEGIN
/i = 123$pid/
{
	exit(0);
}

pid$pid
{
	exit(0);
}
