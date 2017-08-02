/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Repeating the same identifier in the same enumeration will throw a compiler
 * error.
 *
 * SECTION: Type and Constant Definitions/Enumerations
 *
 * NOTES:
 *
 */

#pragma D option quiet

enum colors {
	RED,
	GREEN,
	GREEN = 2,
	BLUE
};

BEGIN
{
	exit(0);
}
