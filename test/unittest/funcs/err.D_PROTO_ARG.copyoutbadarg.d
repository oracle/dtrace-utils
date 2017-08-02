/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  copyout() must handle invalid argument types.
 *
 * SECTION: Actions and Subroutines/copyout()
 *
 */


BEGIN
{
	copyout("not_void_*", "not_uinptr_t", "not_size_t");
	exit(0);
}

