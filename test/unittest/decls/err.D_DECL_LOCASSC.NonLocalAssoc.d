/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * associative arrays may not be declared as local variables
 *
 * SECTION: Errtags/D_DECL_LOCASSC
 *
 */

#pragma D option quiet

this int a[int];

BEGIN
{
	exit(0);
}

ERROR
{
	exit(0);
}
