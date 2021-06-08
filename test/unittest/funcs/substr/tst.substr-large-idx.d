/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The substr() subroutine supports index values well beyond the
 *	      maximum string size.
 *
 * SECTION: Actions and Subroutines/substr()
 */

#pragma D option quiet
#pragma D option strsize=256

BEGIN
{
	trace(substr("abcdefgh", 12345678));
	exit(0);
}
