/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: tick-* fires with approximately the correct period.
 *
 * SECTION: profile Provider/tick-n probes
 */
/* @@tags: unstable */

#pragma D option quiet

BEGIN
{
	last = timestamp;

	expected = 10 * 1000 * 1000; /* nsecs */
	low = expected / 2;
	high = expected * 2;
	toohigh = expected * 20;

	ntotal = 0;
	ngood = 0;
	nbad = 0;
}
tick-10msec
{
	t = timestamp;
	delta = t - last;
	last = t;
	ntotal++;

	/* delta between low and high is good */
	ngood += delta < low ? 0 : (delta > high ? 0 : 1);

	/* delta above toohigh is bad */
	nbad += delta < toohigh ? 0 : 1;
}
tick-10msec
/delta > toohigh/
{
	printf("too high:\n%15d (observed)\n%15d (tolerated)\n",
	   delta, toohigh);
}
tick-25sec
{
	printf("%d counts; %d good and %d bad\n", ntotal, ngood, nbad);
	printf("%s\n", nbad ? "ERROR" : "success");
	exit(nbad != 0);
}
