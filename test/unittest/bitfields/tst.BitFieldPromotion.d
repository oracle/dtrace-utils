/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Bit-field will be automatically promoted to the next largest
 * integer type for use in any expression and then the value assigned will
 * warp around the maximum number assignable to the data type.
 *
 * SECTION: Structs and Unions/Bit-Fields
 */

#pragma D option quiet

struct bitRecord{
	int a : 1;
	int b : 15;
	int c : 31;
} var;

BEGIN
{
	var.a = 256;
	var.b = 65536;
	var.c = 4294967296;

	printf("bitRecord.a: %d\nbitRecord.b: %d\nbitRecord.c: %d\n",
	var.a, var.b, var.c);
	exit(0);
}

END
/(0 != var.a) || (0 != var.b) || (0 != var.c)/
{
	exit(1);
}
