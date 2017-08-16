#!/usr/sbin/dtrace -qs

/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Try to assign value to Macro.
 *
 * SECTION: Scripting
 *
 */

BEGIN
{
	pid = 1;
	exit(0);
}
