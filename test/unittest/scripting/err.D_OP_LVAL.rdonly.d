#!/usr/sbin/dtrace -qs

/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Try to update a read-only macro.
 *
 * SECTION: Scripting
 *
 */

BEGIN
{
	$pid++;
	exit(0);
}
