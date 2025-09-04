/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Enumeration lists cannot have empty enumerations.
 *
 * SECTION: Type and Constant Definitions/Enumerations
 */

#pragma D option quiet

enum one {
	,
	ONE
};

BEGIN
{
	exit(0);
}
