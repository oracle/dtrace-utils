/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *   Generates an associative array reference that should overflow the tuple
 *   register stack.  We should detect and report this at compile time.
 *
 * SECTION: Variables/Associative Arrays
 *
 */

BEGIN
{
	a[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10] = 0;
}
