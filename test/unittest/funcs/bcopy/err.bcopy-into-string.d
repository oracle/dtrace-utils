/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: A bcopy() into an alloca()'d string variable yields an error.
 *
 * SECTION: Actions and Subroutines/bcopy()
 */

BEGIN {
	s = (string)alloca(10);
	bcopy(probename, s, 5);
	exit(0);
}
