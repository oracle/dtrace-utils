/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: A bcopy() into an alloca()'d space can be used as a string.
 *
 * SECTION: Actions and Subroutines/bcopy()
 */

#pragma D option quiet

BEGIN {
	s = alloca(10);
	bcopy(probename, s, 5);
	trace((string)s);
	exit(0);
}
