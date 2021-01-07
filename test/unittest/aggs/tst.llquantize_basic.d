/*
 * Oracle Linux DTrace.
 * Copyright (c) 2017, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  llquantize() performs correctly even for special code paths
 *
 *  These older tests can be reviewed to see which can be removed:
 *    - tst.llquantincr.[d|r]
 *    - tst.llquantize.[d|r]
 *    - tst.llquantneg.[d|r]
 *    - tst.llquantnormal.[d|r]
 *    - tst.llquantoverflow.[d|r]
 *    - tst.llquantround.[d|r]
 *    - tst.llquantstep.[d|r]
 *
 * SECTION: Aggregations/Aggregations
 *
 */

#pragma D option quiet

BEGIN
{
	/* basic functionality for steps<factor */
	@basicA = llquantize(-20, 4, 0, 1, 2, 100-20);
	@basicA = llquantize(-16, 4, 0, 1, 2, 100-16);
	@basicA = llquantize(-12, 4, 0, 1, 2, 100-12);
	@basicA = llquantize( -8, 4, 0, 1, 2, 100- 8);
	@basicA = llquantize( -6, 4, 0, 1, 2, 100- 6);
	@basicA = llquantize( -5, 4, 0, 1, 2, 100- 5);
	@basicA = llquantize( -4, 4, 0, 1, 2, 100- 4);
	@basicA = llquantize( -3, 4, 0, 1, 2, 100- 3);
	@basicA = llquantize( -2, 4, 0, 1, 2, 100- 2);
	@basicA = llquantize( -1, 4, 0, 1, 2, 100- 1);
	@basicA = llquantize(  0, 4, 0, 1, 2, 100+ 0);
	@basicA = llquantize(  1, 4, 0, 1, 2, 100+ 1);
	@basicA = llquantize(  2, 4, 0, 1, 2, 100+ 2);
	@basicA = llquantize(  3, 4, 0, 1, 2, 100+ 3);
	@basicA = llquantize(  4, 4, 0, 1, 2, 100+ 4);
	@basicA = llquantize(  5, 4, 0, 1, 2, 100+ 5);
	@basicA = llquantize(  6, 4, 0, 1, 2, 100+ 6);
	@basicA = llquantize(  8, 4, 0, 1, 2, 100+ 8);
	@basicA = llquantize( 12, 4, 0, 1, 2, 100+12);
	@basicA = llquantize( 16, 4, 0, 1, 2, 100+16);
	@basicA = llquantize( 20, 4, 0, 1, 2, 100+20);

	/* basic functionality for steps==factor */
	@basicB = llquantize(-20, 4, 0, 1, 4, 100-20);
	@basicB = llquantize(-16, 4, 0, 1, 4, 100-16);
	@basicB = llquantize(-12, 4, 0, 1, 4, 100-12);
	@basicB = llquantize( -8, 4, 0, 1, 4, 100- 8);
	@basicB = llquantize( -6, 4, 0, 1, 4, 100- 6);
	@basicB = llquantize( -5, 4, 0, 1, 4, 100- 5);
	@basicB = llquantize( -4, 4, 0, 1, 4, 100- 4);
	@basicB = llquantize( -3, 4, 0, 1, 4, 100- 3);
	@basicB = llquantize( -2, 4, 0, 1, 4, 100- 2);
	@basicB = llquantize( -1, 4, 0, 1, 4, 100- 1);
	@basicB = llquantize(  0, 4, 0, 1, 4, 100+ 0);
	@basicB = llquantize(  1, 4, 0, 1, 4, 100+ 1);
	@basicB = llquantize(  2, 4, 0, 1, 4, 100+ 2);
	@basicB = llquantize(  3, 4, 0, 1, 4, 100+ 3);
	@basicB = llquantize(  4, 4, 0, 1, 4, 100+ 4);
	@basicB = llquantize(  5, 4, 0, 1, 4, 100+ 5);
	@basicB = llquantize(  6, 4, 0, 1, 4, 100+ 6);
	@basicB = llquantize(  8, 4, 0, 1, 4, 100+ 8);
	@basicB = llquantize( 12, 4, 0, 1, 4, 100+12);
	@basicB = llquantize( 16, 4, 0, 1, 4, 100+16);
	@basicB = llquantize( 20, 4, 0, 1, 4, 100+20);

	/* basic functionality for steps>factor */
	@basicC = llquantize(-20, 4, 0, 1, 8, 100-20);
	@basicC = llquantize(-16, 4, 0, 1, 8, 100-16);
	@basicC = llquantize(-12, 4, 0, 1, 8, 100-12);
	@basicC = llquantize( -8, 4, 0, 1, 8, 100- 8);
	@basicC = llquantize( -6, 4, 0, 1, 8, 100- 6);
	@basicC = llquantize( -5, 4, 0, 1, 8, 100- 5);
	@basicC = llquantize( -4, 4, 0, 1, 8, 100- 4);
	@basicC = llquantize( -3, 4, 0, 1, 8, 100- 3);
	@basicC = llquantize( -2, 4, 0, 1, 8, 100- 2);
	@basicC = llquantize( -1, 4, 0, 1, 8, 100- 1);
	@basicC = llquantize(  0, 4, 0, 1, 8, 100+ 0);
	@basicC = llquantize(  1, 4, 0, 1, 8, 100+ 1);
	@basicC = llquantize(  2, 4, 0, 1, 8, 100+ 2);
	@basicC = llquantize(  3, 4, 0, 1, 8, 100+ 3);
	@basicC = llquantize(  4, 4, 0, 1, 8, 100+ 4);
	@basicC = llquantize(  5, 4, 0, 1, 8, 100+ 5);
	@basicC = llquantize(  6, 4, 0, 1, 8, 100+ 6);
	@basicC = llquantize(  8, 4, 0, 1, 8, 100+ 8);
	@basicC = llquantize( 12, 4, 0, 1, 8, 100+12);
	@basicC = llquantize( 16, 4, 0, 1, 8, 100+16);
	@basicC = llquantize( 20, 4, 0, 1, 8, 100+20);

	/* basic functionality for steps>factor and larger lmag */
	/* factor*factor%steps is nonzero, but pow(factor,lmag+1)%steps is 0 */
	@basicD = llquantize(-65536          , 4, 4, 6, 32,    2);
	@basicD = llquantize(-16384          , 4, 4, 6, 32,    4);
	@basicD = llquantize(-16384        +1, 4, 4, 6, 32,    8);
	@basicD = llquantize( -4096- 11*512  , 4, 4, 6, 32,   16);
	@basicD = llquantize( -4096- 11*512+1, 4, 4, 6, 32,   32);
	@basicD = llquantize( -4096- 10*512  , 4, 4, 6, 32,   64);
	@basicD = llquantize( -4096- 10*512+1, 4, 4, 6, 32,  128);
	@basicD = llquantize( -4096-  9*512  , 4, 4, 6, 32,  256);
	@basicD = llquantize( -4096-  9*512+1, 4, 4, 6, 32,  512);
	@basicD = llquantize( -4096-  8*512  , 4, 4, 6, 32, 1024);
	@basicD = llquantize( -4096-  8*512+1, 4, 4, 6, 32, 1024);
	@basicD = llquantize( -4096-  1*512-2, 4, 4, 6, 32, 2048);
	@basicD = llquantize( -4096-  1*512-1, 4, 4, 6, 32, 4096);
	@basicD = llquantize( -1024- 12*128  , 4, 4, 6, 32, 2048);
	@basicD = llquantize( -1024- 12*128+1, 4, 4, 6, 32, 4096);
	@basicD = llquantize( -1024          , 4, 4, 6, 32, 2048);
	@basicD = llquantize(  -256-  3* 32  , 4, 4, 6, 32, 1024);
	@basicD = llquantize(  -256-  3* 32+1, 4, 4, 6, 32,  512);
	@basicD = llquantize(  -256          , 4, 4, 6, 32,  256);
	@basicD = llquantize(  -256        +1, 4, 4, 6, 32,  128);
	@basicD = llquantize(   256        -1, 4, 4, 6, 32,    2);
	@basicD = llquantize(   256          , 4, 4, 6, 32,  256);
	@basicD = llquantize(   256+  3* 32-1, 4, 4, 6, 32,  512);
	@basicD = llquantize(   256+  3* 32  , 4, 4, 6, 32, 1024);
	@basicD = llquantize(  1024          , 4, 4, 6, 32, 2048);
	@basicD = llquantize(  1024+ 12*128-1, 4, 4, 6, 32, 4096);
	@basicD = llquantize(  1024+ 12*128  , 4, 4, 6, 32, 2048);
	@basicD = llquantize(  4096+  1*512+1, 4, 4, 6, 32, 4096);
	@basicD = llquantize(  4096+  1*512+2, 4, 4, 6, 32, 2048);
	@basicD = llquantize(  4096+  8*512-1, 4, 4, 6, 32, 1024);
	@basicD = llquantize(  4096+  8*512  , 4, 4, 6, 32, 1024);
	@basicD = llquantize(  4096+  9*512-1, 4, 4, 6, 32,  512);
	@basicD = llquantize(  4096+  9*512  , 4, 4, 6, 32,  256);
	@basicD = llquantize(  4096+ 10*512-1, 4, 4, 6, 32,  128);
	@basicD = llquantize(  4096+ 10*512  , 4, 4, 6, 32,   64);
	@basicD = llquantize(  4096+ 11*512-1, 4, 4, 6, 32,   32);
	@basicD = llquantize(  4096+ 11*512  , 4, 4, 6, 32,   16);
	@basicD = llquantize( 16384        -1, 4, 4, 6, 32,    8);
	@basicD = llquantize( 16384          , 4, 4, 6, 32,    4);
	@basicD = llquantize( 65536          , 4, 4, 6, 32,    2);

	/* all values are zero */
	@zero  = llquantize(0, 2, 0, 1, 2, +1);
	@zero  = llquantize(0, 2, 0, 1, 2, -1);

	@zeroB = llquantize(0, 2, 0, 1, 4, +1); /* lmag==0 && steps>factor */
	@zeroB = llquantize(0, 2, 0, 1, 4, -1);

	/* values are positive, negative, or both */
	@posvals = llquantize(1, 2, 0, 1, 2, +1);
	@negvals = llquantize(2, 2, 0, 1, 2, -1);
	@allvals = llquantize(1, 2, 0, 1, 2, +1);
	@allvals = llquantize(2, 2, 0, 1, 2, -1);

	/* lmag==hmag (presumably not a special code path) */
	@singlerangeA = llquantize(-4, 4, 0, 0, 4, 100-4);
	@singlerangeA = llquantize(-3, 4, 0, 0, 4, 100-3);
	@singlerangeA = llquantize(-2, 4, 0, 0, 4, 100-2);
	@singlerangeA = llquantize(-1, 4, 0, 0, 4, 100-1);
	@singlerangeA = llquantize( 0, 4, 0, 0, 4, 100  );
	@singlerangeA = llquantize( 1, 4, 0, 0, 4, 100+1);
	@singlerangeA = llquantize( 2, 4, 0, 0, 4, 100+2);
	@singlerangeA = llquantize( 3, 4, 0, 0, 4, 100+3);
	@singlerangeA = llquantize( 4, 4, 0, 0, 4, 100+4);

	@singlerangeB = llquantize(-4, 4, 0, 0, 8, 100-4);
	@singlerangeB = llquantize(-3, 4, 0, 0, 8, 100-3);
	@singlerangeB = llquantize(-2, 4, 0, 0, 8, 100-2);
	@singlerangeB = llquantize(-1, 4, 0, 0, 8, 100-1);
	@singlerangeB = llquantize( 0, 4, 0, 0, 8, 100  );
	@singlerangeB = llquantize( 1, 4, 0, 0, 8, 100+1);
	@singlerangeB = llquantize( 2, 4, 0, 0, 8, 100+2);
	@singlerangeB = llquantize( 3, 4, 0, 0, 8, 100+3);
	@singlerangeB = llquantize( 4, 4, 0, 0, 8, 100+4);

	/* steps==1 (presumably not a special code path) */
	@straightlog = llquantize(-100000, 10, 0, 3, 1, 100108);
	@straightlog = llquantize( -10000, 10, 0, 3, 1, 100208);
	@straightlog = llquantize(  -1000, 10, 0, 3, 1, 100308);
	@straightlog = llquantize(   -100, 10, 0, 3, 1, 100408);
	@straightlog = llquantize(    -10, 10, 0, 3, 1, 100508);
	@straightlog = llquantize(     -1, 10, 0, 3, 1, 100608);
	@straightlog = llquantize(      0, 10, 0, 3, 1, 100708);
	@straightlog = llquantize(      1, 10, 0, 3, 1, 100808);
	@straightlog = llquantize(     10, 10, 0, 3, 1, 100908);
	@straightlog = llquantize(    100, 10, 0, 3, 1, 101008);
	@straightlog = llquantize(   1000, 10, 0, 3, 1, 101108);
	@straightlog = llquantize(  10000, 10, 0, 3, 1, 101208);
	@straightlog = llquantize( 100000, 10, 0, 3, 1, 101308);

	/* and some tests without the optional "increment" argument */

	/* first and last bins are shifted to show a zero-value bin at each end */
	@shift = llquantize(-3, 2, 0, 1, 2);
	@shift = llquantize( 3, 2, 0, 1, 2);

	/* any possible "ghost-bin" problem (lmag==0 && steps>factor) */
	@ghostbin = llquantize(-1, 2, 0, 1, 4);
	@ghostbin = llquantize( 1, 2, 0, 1, 4);

	printf("basicA (steps<factor)\n");
	printa(@basicA);
	printf("basicB (steps==factor)\n");
	printa(@basicB);
	printf("basicC (steps>factor)\n");
	printa(@basicC);
	printf("basicD (steps>factor and bigger lmag)\n");
	printa(@basicD);
	printf("zero\n");
	printa(@zero);
	printf("zeroB (lmag==0 && steps>factor)\n");
	printa(@zeroB);
	printf("posvals\n");
	printa(@posvals);
	printf("negvals\n");
	printa(@negvals);
	printf("allvals\n");
	printa(@allvals);
	printf("singlerangeA\n");
	printa(@singlerangeA);
	printf("singlerangeB\n");
	printa(@singlerangeB);
	printf("straightlog\n");
	printa(@straightlog);
	printf("shift\n");
	printa(@shift);
	printf("ghostbin (lmag==0 && steps>factor); check ghost-bin problem\n");
	printa(@ghostbin);
	exit(0);
}

