#!/usr/sbin/dtrace -s

/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Basic script which uses exit(0) to exit.
 *
 * SECTION: Scripting
 *
 */

BEGIN
{
	exit(0);
}
