/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *
 *  Simple 'this' declaration.
 *
 * SECTION:  Variables/Scalar Variables
 *
 */
this x;

BEGIN
{
	x = "dummy";
	exit(0);
}
