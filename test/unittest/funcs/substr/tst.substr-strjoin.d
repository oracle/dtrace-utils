/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Combining strjoin) and substr() works.
 *
 * SECTION: Actions and Subroutines/strjoin()
 * SECTION: Actions and Subroutines/substr()
 */

#pragma D option quiet

BEGIN
{
	trace(
	    substr(
		strjoin(
		    strjoin(substr(probeprov, 0, 2), substr(probeprov, 2)),
		    strjoin(substr(probename, 0, 3), substr(probename, 3))
		), 6, 3
	    )
	);
	exit(0);
}

ERROR
{
	exit(1);
}
