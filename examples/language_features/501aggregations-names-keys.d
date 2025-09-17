/*
 * Linux DTrace
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#!/usr/sbin/dtrace -s

/*
 *  SYNOPSIS
 *    sudo ./501aggregations-names-keys.d
 *
 *  DESCRIPTION
 *    Aggregations can be named or use keys.
 */

tick-20hz
{
	@my_count[(timestamp % 2 == 0) ? "even" : "odd"] = count();
}

tick-1sec
{
	exit(0);
}
