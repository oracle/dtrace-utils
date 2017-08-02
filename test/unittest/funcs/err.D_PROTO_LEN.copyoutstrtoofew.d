/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *    Calling copyoutstr() with less than 2 arguments will generate an error
 *
 * SECTION: Actions and Subroutines/copyoutstr()
 *
 */


BEGIN
{
	copyoutstr("string");
	exit(0);
}

