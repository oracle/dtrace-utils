/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * array tuples may not use variable-length argument lists
 *
 * SECTION: Errtags/ D_DECL_ARRVA
 *
 */

#pragma D option quiet


int a[...];

BEGIN
{
	exit(0);
}

ERROR
{
	exit(0);
}
