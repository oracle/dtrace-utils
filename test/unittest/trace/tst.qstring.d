/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

/*
 * ASSERTION:
 *  Positive trace tests - trace with strings, using quiet option
 *
 * SECTION: Actions and Subroutines/trace()
 */


#pragma D option quiet

BEGIN
{
	trace("this");
	trace(" %should work.\n");
	trace("%don't w%orry -- this won't cause a %segfault.\n");

	exit(0);
}
