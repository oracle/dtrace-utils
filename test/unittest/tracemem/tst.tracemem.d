/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Results for tracemem() are correct.
 *
 * SECTION: Actions and Subroutines/tracemem()
 */

#pragma D option quiet

BEGIN {
	p = (char *)&`linux_banner;	/* first 14 chars are "Linux version " */

	/* try tracemem() with various sizes */

	tracemem(p, 14);		/* "Linux version ": 14 bytes */
	tracemem(p, 14, -1);		/* "Linux version ": 14 bytes (arg2 < 0) */
	tracemem(p, 14,  0);		/*                    0 bytes */
	tracemem(p, 14,  2);		/* "Li"            :  2 bytes */
	tracemem(p, 14,  7);		/* "Linux v"       :  7 bytes */
	tracemem(p, 14, 12);		/* "Linux versio"  : 12 bytes */
	tracemem(p, 14, 14);		/* "Linux version ": 14 bytes */
	tracemem(p, 14, 15);		/* "Linux version ": 14 bytes (arg1 <= arg2) */

	exit(0);
}
