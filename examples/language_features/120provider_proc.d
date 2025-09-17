/*
 * Linux DTrace
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#!/usr/sbin/dtrace -s

/*
 *  SYNOPSIS
 *    sudo ./120provider_proc.d
 *
 *  DESCRIPTION
 *    We can track when processes throughout their life cycle.
 */

proc:::create
{
	printf("%s %s\n", probename, args[0]->pr_fname);
}

proc:::exec
{
	printf("%s %s\n", probename, args[0]);
}

proc:::start
{
	printf("%s %s\n", probename, execname);
}

proc:::exit
{
	printf("%s %s\n", probename, execname);
}

/*
 *  A tick probe is used to fire once to stop data collection.
 */
profile:::tick-10sec
{
	exit(0);
}
