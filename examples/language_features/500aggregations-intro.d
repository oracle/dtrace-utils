/*
 * Linux DTrace
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#!/usr/sbin/dtrace -s

#pragma D option quiet

/*
 *  SYNOPSIS
 *    sudo ./500aggregations-intro.d
 *
 *  DESCRIPTION
 *    Aggregations can be used to collect statistical data on
 *    some value.  Results are printed by default when the
 *    script terminates.
 */

tick-20hz
{
	/* do not even bother naming the aggregation;  just "@" */
	@ = count();
}

tick-1sec
{
	exit(0);
}
