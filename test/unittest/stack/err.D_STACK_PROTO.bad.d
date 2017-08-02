/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	stack() accepts one argument
 *
 * SECTION: Actions and Subroutines/stack()
 *
 */


BEGIN
{
	stack(1, 2);
	exit(0);
}

