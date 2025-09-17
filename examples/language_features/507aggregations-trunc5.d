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
 *    sudo ./507aggregations-trunc5.d
 *
 *  DESCRIPTION
 *    Here, is another use of trunc() -- with an additional
 *    argument to focus attention on the top values.
 */

BEGIN
{
	/*
	 * The mask will change size.  When it is wide, it will
	 * spread values over many keys.  When it is narrow, it
	 * will concentrate values over a few keys.  So, some
	 * keys will get many values, while others will get few.
	 */
	mask = 255;
}

tick-100hz
{
	/* the aggregation */
	@[timestamp & mask] = count();

	/* update the mask */
	mask >>= 1;
	mask = (mask == 0) ? 255 : mask;
}

tick-1sec
{
	printf("entire result\n");
	printa("    %3d %@3d\n", @);

	trunc(@, 5);

	printf("top 5 keys\n");
	printa("    %3d %@3d\n", @);

	/*
	 * On exit, we will no longer print the aggregation
	 * by default, since it was just printed.
	 */
	exit(0);
}
