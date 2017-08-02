/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/*
 * ASSERTION: Cannot redeclare identifier using typedef.
 *
 * SECTION: Type and Constant Definitions/Typedef
 *
 * NOTES:
 *
 */

#pragma D option quiet

typedef int new_int;

BEGIN
{
	exit(0);
}

typedef char new_int;

END
{
}

