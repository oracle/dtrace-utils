/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Positive aggregation key test
 *
 * SECTION: Aggregations/Aggregations
 */

#pragma D option quiet

BEGIN
{
	i = 0;
}

tick-10ms
/i != 5/
{
	i++;
	@counts[execname, pid, id, tid, arg0, i ] = count();

}

tick-10ms
/i == 5/
{
	exit(0);
}
