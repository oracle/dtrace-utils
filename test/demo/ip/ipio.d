/*
 * Oracle Linux DTrace.
 * Copyright (c) 2008, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

#pragma D option quiet
#pragma D option switchrate=10hz

dtrace:::BEGIN
{
	printf(" %3s %10s %15s    %15s %8s %6s\n", "CPU", "DELTA(us)",
	    "SOURCE", "DEST", "INT", "BYTES");
	last = timestamp;
}

ip:::send
{
	this->elapsed = (timestamp - last) / 1000;
	printf(" %3d %10d %15s -> %15s %8s %6d\n", cpu, this->elapsed,
	    args[2]->ip_saddr, args[2]->ip_daddr, args[3]->if_name,
	    args[2]->ip_plength);
	last = timestamp;
}

ip:::receive
{
	this->elapsed = (timestamp - last) / 1000;
	printf(" %3d %10d %15s <- %15s %8s %6d\n", cpu, this->elapsed,
	    args[2]->ip_daddr, args[2]->ip_saddr, args[3]->if_name,
	    args[2]->ip_plength);
	last = timestamp;
}

tick-10s
{
	exit(0);
}

