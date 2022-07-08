/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

BEGIN
{
	a = 7;
	b = 13;
	val = (-a * b) + a;
}

tick-1ms
{
	incr = val % b;
	val += a;
}

tick-1ms
/val == 0/
{
	val += a;
}

tick-1ms
/incr != 0/
{
	i++;
	@one[i] = lquantize(0, 10, 20, 1, incr);
	@two[i] = lquantize(0, 1, 20, 5, incr);
	@three[i] = lquantize(0, 0, 20, 1, incr);
	@four[i] = lquantize(0, -10, 10, 1, incr);
	@five[i] = lquantize(0, -10, 0, 1, incr);
	@six[i] = lquantize(0, -10, -1, 1, incr);
	@seven[i] = lquantize(0, -10, -2, 1, incr);
}

tick-1ms
/incr == 0/
{
	printf("Zero below the range:\n");
	printa(@one);
	printf("\n");

	printf("Zero just below the range:\n");
	printa(@two);
	printf("\n");

	printf("Zero at the bottom of the range:\n");
	printa(@three);
	printf("\n");

	printf("Zero within the range:\n");
	printa(@four);
	printf("\n");

	printf("Zero at the top of the range:\n");
	printa(@five);
	printf("\n");

	printf("Zero just above the range:\n");
	printa(@six);
	printf("\n");

	printf("Zero above the range:\n");
	printa(@seven);
	printf("\n");

	exit(0);
}
