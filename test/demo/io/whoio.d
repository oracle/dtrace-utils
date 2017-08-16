/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

io:::start
{
	@[args[1]->dev_statname, execname, pid] = sum(args[0]->b_bcount);
}

END
{
	printf("%10s %20s %10s %15s\n", "DEVICE", "APP", "PID", "BYTES");
	printa("%10s %20s %10d %15@d\n", @);
}
