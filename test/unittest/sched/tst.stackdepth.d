/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option switchrate=100hz

sched:::on-cpu
/stackdepth > 0/
{
	exit(0);
}

tick-1s
/i++ == 3/
{
	exit(1);
}
