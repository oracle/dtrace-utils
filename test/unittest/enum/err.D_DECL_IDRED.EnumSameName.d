/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Enumerations assigning same or different values to the same identifier in
 *  different enumerations should throw a compiler error.
 *
 * SECTION: Type and Constant Definitions/Enumerations
 *
 *
 */

#pragma D option quiet

enum colors {
	RED,
	GREEN,
	BLUE
};

enum shades {
	RED,
	ORANGE,
	GREEN,
	WHITE
};

BEGIN
{
	exit(0);
}
