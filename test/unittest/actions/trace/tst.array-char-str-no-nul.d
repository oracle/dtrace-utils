/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The trace() action prints a char-array of printable characters
 *	      (not terminated) correctly as a string.
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
	n[5] = 'C';
	n[6] = 'd';
	n[7] = 'D';
	n[8] = 'e';
	trace(n);
	exit(0);
}
