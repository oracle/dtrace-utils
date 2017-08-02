/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Verify invalid attributes
 *
 * SECTION: Pragmas;
 *	Stabilty/Interface Attributes
 */


inline int foo

#pragma D attributes incorrect_attr foo

BEGIN
{
	exit(0);
}
