#!/usr/sbin/dtrace -s

/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: none */

BEGIN
{
	trace("hello");
	exit(0);
}
