/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Cannot take the address of a bit-field member using the &
 * operator.
 *
 * SECTION: Structs and Unions/Bit-Fields
 */

#pragma D option quiet

struct bitRecord{
	int a : 1;
	int b : 3;
	int c : 12;
} var;

BEGIN
{
	printf("address of a: %d\naddress of b: %d\naddress of c: %dn",
	&var.a, &var.b, &var.c);

	exit(0);
}
