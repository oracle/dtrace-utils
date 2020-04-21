/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Enumerations assigning the same value to different identifiers in the same
 * enumeration should work okay.
 *
 * SECTION: Type and Constant Definitions/Enumerations
 *
 * NOTES:
 *
 */

#pragma D option quiet

enum colors {
	RED = 2,
	GREEN = 2,
	BLUE = 2
};

BEGIN
{
	exit(0);
}
