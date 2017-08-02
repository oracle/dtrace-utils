/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	aggsortkeypos option works when sorting by values, values are
 * 	equal, and keys are compared to break the tie
 *
 * SECTION: Aggregations, Printing Aggregations
 *
 */

#pragma D option quiet
#pragma D option aggsortkeypos=1

BEGIN
{
	@[1, 3] = sum(0);
	@[2, 2] = sum(0);
	@[3, 1] = sum(0);

	exit(0);
}
