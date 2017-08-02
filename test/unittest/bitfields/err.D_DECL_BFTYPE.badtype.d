/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */


/*
 * ASSERTION:
 *	Non-integer types used for bitfields will result in a D_DECL_BFTYPE
 *	error.
 *
 * SECTION: Structs and Unions/Bit-Fields
 */

#pragma D option quiet

struct bits {
	float : 1;
} xyz;

BEGIN
{
	exit(1);
}
