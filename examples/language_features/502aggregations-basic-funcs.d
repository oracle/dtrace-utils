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
 *    sudo ./501aggregations-basic-funcs.d
 *
 *  DESCRIPTION
 *    Aggregations can include basic statistical functions.
 */

tick-20hz
{
	@my_count["count"] = count();
	@my_sum["sum"] = sum(timestamp);
	@my_avg["average"] = avg(timestamp);
	@my_stddev["standard deviation"] = stddev(timestamp);
	@my_min["minimum"] = min(timestamp);
	@my_max["maximum"] = max(timestamp);
}

tick-1sec
{
	exit(0);
}
