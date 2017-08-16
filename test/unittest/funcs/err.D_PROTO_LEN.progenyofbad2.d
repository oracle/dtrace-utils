/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	progenyof() should return an error if the argument is an
 *	incorrect type.
 *
 * SECTION: Actions and Subroutines/progenyof()
 *
 */


BEGIN
{
	progenyof(trace());
	exit(0);
}

