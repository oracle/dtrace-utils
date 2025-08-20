#!/usr/sbin/dtrace -s

# pragma D option quiet

/*
 *  SYNOPSIS
 *    sudo ./508aggregations-normalize.d
 *
 *  DESCRIPTION
 *    Here, is another use of trunc() -- with an additional
 *    argument to focus attention on the top values.
 */

BEGIN
{
	treport = timestamp;
	ttick = timestamp;
}

/* add the number of nanoseconds since the last tick */
tick-100hz
{
	@ = sum(timestamp - ttick);
	ttick = timestamp;
}

/* report at erratic intervals */
tick-3100ms,
tick-3600ms
{
	/* determine number of usecs since last report */
	this->us_since_last = (timestamp - treport) / 1000;
	treport = timestamp;
	printf("%8d usecs since the last report\n", this->us_since_last);

	/* normalize the total number of nsecs by the number of usecs */
	normalize(@, this->us_since_last);

	/* report aggregation (do not worry about formatting) */
	printa(@);

	/* clear the aggregation before resuming */
	clear(@);
}

tick-12sec
{
	exit(0);
}
