/*
 * Oracle Linux DTrace.
 * Copyright (c) 2012, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: none */

cpuinfo_t	*cpuinfo;

profile-10ms
{
	cpuinfo = curcpu;
	trace(cpuinfo->cpu_id);
	n++;
}

profile-10ms
/n > 2/
{
	exit(0);
}

tick-1s
{
	exit(1);
}
