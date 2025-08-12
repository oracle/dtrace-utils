/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The trace() action prints a non-char array correctly as raw bytes.
 *
 * SECTION: Actions and Subroutines/trace()
 */

short n[5];

BEGIN
{
	n[0] = 0x7464;
	n[1] = 0x6172;
	n[2] = 0x6563;
	n[3] = 0x1234;
	n[4] = 0x4321;
	trace(n);
	exit(0);
}
