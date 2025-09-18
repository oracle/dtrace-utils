/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Enumerations using names also used for enumerators in the kernel should not
 * raise errors.
 *
 * SECTION: Type and Constant Definitions/Enumerations
 *
 * NOTES:
 *
 */

#pragma D option quiet

enum dirs {
	UP,
	DOWN
};

BEGIN
{
	exit(0);
}
