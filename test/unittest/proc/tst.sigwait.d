/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@xfail: dtv2 */
/* @@trigger: proc-tst-sigwait */

#pragma D option destructive

proc:::signal-send
/pid == 0 && args[1]->pr_pid == $target && args[2] == SIGUSR1/
{
	sent = 1;
}

proc:::signal-clear
/pid == $target && args[0] == SIGUSR1 && sent/
{
	exit(0);
}
