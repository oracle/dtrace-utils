#!/usr/sbin/dtrace -s

/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	To use trace in a script
 *
 * SECTION: Scripting
 *
 */

BEGIN
{
	trace("hello");
	exit(0);
}
