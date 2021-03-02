/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Bit-field store and load work properly.
 *
 * SECTION: Structs and Unions/Bit-Fields
 */

#pragma D option quiet

struct bitRecord{
	unsigned int  a : 3;
	unsigned int  b : 7;
	unsigned int  c : 14;
	unsigned long d : 35;
	unsigned int  e : 22;
	unsigned int  f : 32;
	unsigned int  g : 16;
	unsigned int  h : 8;
} var;

BEGIN
{
	/* sign bit is 0 */
	var.a = 3;
	var.b = 0x3f;
	var.c = 0x1fff;
	var.d = 0x3ffffffff;
	var.e = 0x1fffff;
	var.f = 0x7fffffff;
	var.g = 0x7fff;
	var.h = 0x7f;
	printf("%d %d %d %d %d %d %d %d\n",
	    var.a, var.b, var.c, var.d, var.e, var.f, var.g, var.h);

	/* sign bit is 1 */
	var.a = 7;
	var.b = 0x75;
	var.c = 0x3555;
	var.d = 0x755555555;
	var.e = 0x355555;
	var.f = 0xf5555555;
	var.g = 0xf555;
	var.h = 0xf5;
	printf("%d %d %d %d %d %d %d %d\n",
	    var.a, var.b, var.c, var.d, var.e, var.f, var.g, var.h);

	exit(0);
}
