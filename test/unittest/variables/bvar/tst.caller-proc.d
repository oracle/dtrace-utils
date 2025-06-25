/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/*
 * @@trigger: periodic_output
 */

#pragma D option quiet

/*
 * Some probes are implemented in terms of rawtp probes.  Avoid them
 * due to stack-skip issues on older kernels.
 */
proc:::exec,
proc:::exec-failure,
proc:::exec-success,
proc:::lwp-start,
proc:::signal-clear,
proc:::signal-send,
proc:::start
{
	stack(2);
	sym(caller);
	exit(0);
}

ERROR
{
	printf("error encountered\n");
	exit(1);
}
