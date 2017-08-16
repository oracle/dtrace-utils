#!/usr/sbin/dtrace -qs

/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Attempt to concat and assign a macro to a variable
 *
 * SECTION: Scripting
 *
 */

BEGIN
{
	i = 123$pid;
	exit(0);
}
