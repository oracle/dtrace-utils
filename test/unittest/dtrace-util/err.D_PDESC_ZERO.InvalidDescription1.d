/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: An invalid probe description throws a D_PDESC_ZERO error.
 *
 * SECTION: Errtags/D_PDESC_ZERO
 *
 */

#pragma D option quiet

fbt:bippity:boppity:boo
{
	exit(0);
}

