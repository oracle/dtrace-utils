/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *
 * Simple !defined logical-or(||) and logical-and(&&) test.
 *
 * SECTION: Program Structure/Use of the C Preprocessor
 *
 */

#if !defined (FLAG) || !defined(STATUS) && !defined(STATE)
#define VALUE 0
#else
#define VALUE 1
#endif

#pragma D option quiet

tick-10ms
{
	printf("The value is %d\n", VALUE);
	exit(0);
}
