/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Bit-field width must be of a number of bits not larger than
 * that of the corresponding base type.
 *
 * SECTION: Structs and Unions/Bit-Fields
 */

#pragma D option quiet

struct bitRecord1{
	char a : 10;
} var1;

struct bitRecord2{
	short a : 33;
} var2;

struct bitRecord3{
	long long a : 65;
} var3;

BEGIN
{
	exit(0);
}
