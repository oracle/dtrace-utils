/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	ustack() accepts two arguments
 *
 * SECTION: User Process Tracing/ustack();
 *	Actions and Subroutines/ustack()
 *
 */


BEGIN
{
	ustack(1, 10, "badarg");
	exit(0);
}

