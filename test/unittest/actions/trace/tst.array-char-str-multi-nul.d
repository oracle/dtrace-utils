/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The trace() action prints a char-array of printable characters
 *	      followed y by multiple 0-bytes correctly.
 *
 * SECTION: Actions and Subroutines/trace()
 */

char n[9];

BEGIN
{
	n[0] = 'a';
	n[1] = 'A';
	n[2] = 'b';
	n[3] = 'B';
	n[4] = 'c';
	n[5] = 0;
	n[6] = 0;
	n[7] = 0;
	n[8] = 0;
	trace(n);
	exit(0);
}
