/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The printf action supports '%s' for alloca()'d string variables.
 *
 * SECTION: Actions/printf()
 */

#pragma D option quiet

BEGIN
{
	s = alloca(10);
	bcopy(probename, s, 5);
	printf("'%s'", (string)s);
	exit(0);
}
