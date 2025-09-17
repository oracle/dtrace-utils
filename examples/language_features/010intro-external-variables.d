/*
 * Linux DTrace
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#!/usr/sbin/dtrace -s

#pragma D option quiet
#pragma D option strsize=64

/*
 *  SYNOPSIS
 *    sudo ./010intro-external-variables.d
 *
 *  DESCRIPTION
 *    We can examine external variables -- that is, variables from outside
 *    the D program, such as kernel variables.  They are distinguished by
 *    a back quote ` and constitute a different name space from that of
 *    the D variables.  When such a variables is named, the kernel and all
 *    loaded modules are searched for the name.  Or, you can specify a
 *    module before the back quote.  The kernel itself is module "vmlinux".
 *    D scripts cannot modify values of kernel variables.
 *
 *    Such external variables are not part of D and are subject to change.
 */

dtrace:::BEGIN
{

	printf("\n==== print some kernel variables ====\n\n");

	printf("cad_pid->level: %d\n", ((struct pid *)`cad_pid)->level);
	printf("crashing_cpu: %d\n", `crashing_cpu);
	printf("linux_banner: %s\n", (string)`linux_banner);
	printf("linux_proc_banner: %s\n", (string)`linux_proc_banner);
	printf("load averages: %d.%02d, %d.%02d, %d.%02d\n",
	    `avenrun[0] / 2048, ((`avenrun[0] % 2048) * 100) / 2048,
	    `avenrun[1] / 2048, ((`avenrun[1] % 2048) * 100) / 2048,
	    `avenrun[2] / 2048, ((`avenrun[2] % 2048) * 100) / 2048);
	printf("max_pvn: %d\n", `max_pfn);
	printf("nr_cpu_ids: %d\n", `nr_cpu_ids);
	printf("saved_command_line: %s\n", (string)`saved_command_line);

	printf("\n==== reprint some of them, explicitly naming vmlinux ====\n\n");

	printf("crashing_cpu: %d\n", vmlinux`crashing_cpu);
	printf("linux_banner: %s\n", (string)vmlinux`linux_banner);
	printf("linux_proc_banner: %s\n", (string)vmlinux`linux_proc_banner);
	printf("max_pvn: %d\n", vmlinux`max_pfn);
	printf("nr_cpu_ids: %d\n", vmlinux`nr_cpu_ids);

	printf("\n==== refer to a data type in a module ====\n\n");

	printf("offset of atomic in struct rds_message: %d\n",
	    offsetof(struct rds`rds_message, atomic));

	exit(0);
}
