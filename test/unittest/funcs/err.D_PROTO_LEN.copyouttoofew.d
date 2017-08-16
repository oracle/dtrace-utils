/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *    Calling copyout() with less than 3 arguments will generate an error
 *
 * SECTION: Actions and Subroutines/copyout()
 *
 */


BEGIN
{
	copyout(0, 20);
	exit(0);
}

