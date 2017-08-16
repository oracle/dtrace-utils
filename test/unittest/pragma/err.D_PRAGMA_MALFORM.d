/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Verify malformed pragma argument list
 *
 * SECTION: Pragmas
 */


#pragma D attributes incorrect_attr

BEGIN
{
	exit(0);
}
