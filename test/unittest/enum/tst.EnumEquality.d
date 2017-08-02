/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Test the identifiers in different D enumerations at same position for
 * equality.
 *
 * SECTION: Type and Constant Definitions/Enumerations
 */

#pragma D option quiet

enum colors {
	RED,
	GREEN,
	BLUE
};

enum shades {
	WHITE,
	BLACK,
	YELLOW
};


profile:::tick-1sec
/(WHITE == RED) && (YELLOW == BLUE) && (GREEN == BLACK)/
{
	exit(0);
}
