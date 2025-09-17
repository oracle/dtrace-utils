/*
 * Linux DTrace
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#!/usr/sbin/dtrace -s

/*
 *  SYNOPSIS
 *    sudo ./130provider_sched_on_off_cpu.d
 *
 *  DESCRIPTION
 *    We can track when processes start and end execution
 *    on a CPU.
 */

sched:::on-cpu
{
	printf("on CPU %d is %s\n", curcpu->cpu_id, curlwpsinfo->pr_name);
}

sched:::off-cpu
{
	printf("off CPU %d is %s\n", curcpu->cpu_id, args[0]->pr_name);
}

/*
 *  A tick probe is used to fire once to stop data collection.
 */
profile:::tick-2sec
{
	exit(0);
}
