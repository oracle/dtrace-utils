/*
 * Oracle Linux DTrace.
 * Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Positive stddev() test
 *
 * SECTION: Aggregations/Aggregations
 *
 * NOTES: This is a simple verifiable positive test of the stddev() function.
 */

#pragma D option quiet

BEGIN
{
	@a = stddev(          0); @a = stddev(          0);
	@b = stddev(       0x10); @b = stddev(       0x20);
	@c = stddev(      0x100); @c = stddev(      0x200);
	@d = stddev(     0x1000); @d = stddev(     0x2000);
	@e = stddev(    0x10000); @e = stddev(    0x20000);
	@f = stddev(   0x100000); @f = stddev(   0x200000);
	@g = stddev(  0x1000000); @g = stddev(  0x2000000);
	@h = stddev( 0x10000000); @h = stddev( 0x20000000);
	@i = stddev( 0x20000000); @i = stddev( 0x40000000);
	@j = stddev( 0x40000000); @j = stddev( 0x80000000);
	@k = stddev( 0x80000000); @k = stddev(0x100000000);
	@l = stddev(0x100000000); @l = stddev(0x200000000);
	printa("%9@x\n", @a);
	printa("%9@x\n", @b);
	printa("%9@x\n", @c);
	printa("%9@x\n", @d);
	printa("%9@x\n", @e);
	printa("%9@x\n", @f);
	printa("%9@x\n", @g);
	printa("%9@x\n", @h);
	printa("%9@x\n", @i);
	printa("%9@x\n", @j);
	printa("%9@x\n", @k);
	printa("%9@x\n", @l);
	exit(0);
}
