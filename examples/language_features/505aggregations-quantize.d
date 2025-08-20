#!/usr/sbin/dtrace -s

# pragma D option quiet

/*
 *  SYNOPSIS
 *    sudo ./505aggregations-quantize.d
 *
 *  DESCRIPTION
 *    While avg(), stddev(), min(), and max() are useful for
 *    characterizing the distribution of values crudely, one
 *    may also be interested in more detail on a distribution.
 *
 *    Aggregations can count by value after the value has been
 *    "quantized" -- that is, grouped into bins.
 */

BEGIN
{
	/* time origin at the beginning of the script's execution */
	t0 = timestamp;

	/* for this example, use some dummy value that will change in scale */
	dummy = t0;
}

tick-20hz
{
	/*
	 * If you have no sense for the distribution of values --
	 * not even of the scale -- then quantize(val) can be a
	 * good starting point.  Its only input parameter is the
	 * value.  Bins are powers of two -- you reduce values
	 * log2() -- giving you a sense of the scale of the values.
	 */
	@crude = quantize(dummy >>= 1);

	/*
	 * But perhaps you know the scale and want to chop some
	 * range of values up linearly.  So we use lquantize().
	 */
	@linear = lquantize(
	    (timestamp - t0) / 1000000,	/* number of msecs since D script start */
	    0,				/* lower bound */
	    2000,			/* upper bound (msecs in 2 secs) */
	    200);			/* number per bin */

	/*
	 * Finally, and this is a little complicated, what if you
	 * liked the original, logarithmic quantization, but you
	 * just want a little more detail.  Then, there is a
	 * log-linear quantization.
	 */
	@log_linear = llquantize(
	    dummy,	/* number of msecs since some arbitrary time in the past */
	    100,	/* use log10() */
	    3,		/* lower bound is 10 to the 3 = 1000 */
	    5,		/* upper bound is 10 to the 9 = 1000000000 */
	    4);		/* number of steps per logarithmic range */
}

tick-2sec
{
	exit(0);
}
