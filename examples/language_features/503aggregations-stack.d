/*
 * Linux DTrace
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#!/usr/sbin/dtrace -s

# pragma D option quiet

/*
 *  SYNOPSIS
 *    sudo ./503aggregations-stack.d
 *
 *  DESCRIPTION
 *    An interesting key to use is "stack()", giving you
 *    information, for example, about hot kernel call stacks.
 *    Each distinct kernel call stack found will be reported
 *    along with the number of times it was encountered.
 */

tick-20hz
{
	@[stack()] = count();
}

tick-1sec
{
	exit(0);
}
