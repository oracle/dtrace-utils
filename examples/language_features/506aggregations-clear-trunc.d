#!/usr/sbin/dtrace -s

# pragma D option quiet

/*
 *  SYNOPSIS
 *    sudo ./506aggregations-clear-trunc.d
 *
 *  DESCRIPTION
 *    Here, we show clear() and trunc(), used with aggregations.
 *
 *    One reason to use printa() is to get aggregation values
 *    periodically while the D script is executing.  In that
 *    case, however, you may want to reset the aggregations.
 *    There are two ways of doing so.
 */

tick-20hz
{
	/* in this example, consider two aggregations */

	/*
	 * "parity" counts whether the timestamp is even or odd.
	 * In each one-second time interval, both cases should
	 * get some values.
	 */
	@parity[(timestamp & 1) ? "odd" : "even"] = count();

	/*
	 * "low_bits" counts how often the low bits (0-255) appear.
	 * It is reasonable that some keys will not reappear.
	 */
	@low_bits[timestamp & 0xff] = count();
}

tick-1sec
{
	/*
	 * Every second, we print the values for the recent
	 * one-second interval.
	 */
	printa("parity: %-4s %@2d\n", @parity);
	printa("low bits: %3d %@d\n", @low_bits);

	/*
	 * We clear out the "parity" values, leaving the keys,
	 * since almost surely those keys will recur.  And if
	 * a key ends up with no counts, we want to know about it!
	 */
	clear(@parity);

	/*
	 * We trunc() (truncate) "low_bits", however, since keys
	 * might not reoccur.  Unused keys take up memory and
	 * pollute the periodic output.
	 */
	trunc(@low_bits);
}

tick-4sec
{
	exit(0);
}
