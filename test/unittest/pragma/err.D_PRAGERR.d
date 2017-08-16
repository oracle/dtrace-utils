/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Verify pragma errors
 *
 * SECTION: Pragmas
 */


#pragma D error "this is an error message"

BEGIN
{
	exit(0);
}
