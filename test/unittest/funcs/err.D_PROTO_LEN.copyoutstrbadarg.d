/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  copyoutstr() must handle invalid argument types.
 *
 * SECTION: Actions and Subroutines/copyoutstr()
 *
 */


BEGIN
{
	copyoutstr("string", "not_uintptr_t");
	exit(0);
}

