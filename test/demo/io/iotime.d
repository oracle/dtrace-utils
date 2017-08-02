/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

BEGIN
{
	printf("%10s %58s %2s %7s\n", "DEVICE", "FILE", "RW", "MS");
}

io:::start
{
	start[args[0]->b_edev, args[0]->b_blkno] = timestamp;
}

io:::done
/start[args[0]->b_edev, args[0]->b_blkno]/
{
	this->elapsed = timestamp - start[args[0]->b_edev, args[0]->b_blkno];
	printf("%10s %58s %2s %3d.%03d\n", args[1]->dev_statname,
	    args[2]->fi_pathname, args[0]->b_flags & B_READ ? "R" : "W",
	    this->elapsed / 1000000, (this->elapsed / 1000) % 1000);
	start[args[0]->b_edev, args[0]->b_blkno] = 0;
}
