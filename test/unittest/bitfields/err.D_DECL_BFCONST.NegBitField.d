/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Bit-field width must be positive.
 *
 * SECTION: Structs and Unions/Bit-Fields
 */

#pragma D option quiet

struct bitRecord{
	int a : 1;
	int b : -3;
	int c : 12;
} var;

BEGIN
{
	exit(0);
}
