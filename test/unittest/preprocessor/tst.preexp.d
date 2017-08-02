/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *
 * Simple #define and predicates test.
 *
 * SECTION: Program Structure/Use of the C Preprocessor
 *
 */

#define MAXVAL 10 * 15


#pragma D option quiet

tick-10ms
/MAXVAL > 5/
{
	printf("The value is %d\n", MAXVAL);
	exit(0);
}
