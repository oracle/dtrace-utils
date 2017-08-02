/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *
 * Multiple storage declarations.
 *
 * SECTION:  Variables/Scalar Variables
 *
 */
this self int x;

BEGIN
{
	x = "dummy";
}
